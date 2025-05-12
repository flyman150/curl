#include <iostream>
#include <string>
#include <fstream>
#include <curl/curl.h>
#include <sys/stat.h>
#include <regex>
#include <stdexcept>

class HttpDownloader {
private:
    std::string url;
    std::string filename;
    FILE* fp;
    CURL* curl;
    bool resume;
    curl_off_t offset;

    // URL验证函数
    bool validateUrl(const std::string& url) {
        // 基本URL格式验证
        std::regex url_pattern(
            R"(^(http|https|ftp)://)"  // 协议
            R"([^\s/$.?#].[^\s]*$)"    // 主机名和路径
        );
        
        if (!std::regex_match(url, url_pattern)) {
            return false;
        }

        // 使用CURL验证URL
        CURL* curl = curl_easy_init();
        if (!curl) {
            return false;
        }

        CURLcode res;
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_NOBODY, 1L);         // 只检查头部
        curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1L);    // HTTP错误时返回失败
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);       // 设置超时时间

        res = curl_easy_perform(curl);
        curl_easy_cleanup(curl);

        return (res == CURLE_OK);
    }

    // 检查文件名是否合法
    bool validateFilename(const std::string& filename) {
        if (filename.empty()) {
            return false;
        }

        // 检查文件名长度
        if (filename.length() > 255) {
            return false;
        }

        // 检查非法字符
        std::regex invalid_chars(R"([<>:"/\\|?*])");
        if (std::regex_search(filename, invalid_chars)) {
            return false;
        }

        return true;
    }

    // 从URL中提取默认文件名
    std::string extractFilenameFromUrl(const std::string& url) {
        size_t pos = url.find_last_of('/');
        if (pos != std::string::npos && pos < url.length() - 1) {
            std::string name = url.substr(pos + 1);
            // 移除URL参数
            size_t param_pos = name.find('?');
            if (param_pos != std::string::npos) {
                name = name.substr(0, param_pos);
            }
            return name;
        }
        return "downloaded_file";
    }

    // 写入回调函数
    static size_t writeCallback(void* ptr, size_t size, size_t nmemb, void* stream) {
        return fwrite(ptr, size, nmemb, (FILE*)stream);
    }

    // 进度回调函数
    static int progressCallback(void* clientp, curl_off_t dltotal, curl_off_t dlnow, 
                              curl_off_t ultotal, curl_off_t ulnow) {
        if (dltotal > 0) {
            double percent = (double)dlnow / (double)dltotal * 100.0;
            printf("\rProgress: %.2f%%", percent);
            fflush(stdout);
        }
        return 0;
    }

public:
    HttpDownloader(const std::string& url, const std::string& filename = "") 
        : fp(nullptr), curl(nullptr), resume(false), offset(0) {
        
        // 初始化CURL
        curl_global_init(CURL_GLOBAL_ALL);

        // 验证URL
        if (!validateUrl(url)) {
            curl_global_cleanup();
            throw std::invalid_argument("Invalid URL: " + url);
        }
        this->url = url;

        // 处理文件名
        if (filename.empty()) {
            this->filename = extractFilenameFromUrl(url);
        } else {
            if (!validateFilename(filename)) {
                curl_global_cleanup();
                throw std::invalid_argument("Invalid filename: " + filename);
            }
            this->filename = filename;
        }

        // 初始化CURL句柄
        curl = curl_easy_init();
        if (!curl) {
            curl_global_cleanup();
            throw std::runtime_error("Failed to initialize CURL");
        }

        // 设置一些基本的CURL选项
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);    // 允许重定向
        curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 5L);         // 最大重定向次数
        curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 10L);   // 连接超时时间
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);    // 验证SSL证书
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);    // 验证主机名
    }

    ~HttpDownloader() {
        if (fp) {
            fclose(fp);
        }
        if (curl) {
            curl_easy_cleanup(curl);
        }
        curl_global_cleanup();
    }

    bool download() {
        if (!curl) {
            std::cerr << "CURL initialization failed!" << std::endl;
            return false;
        }

        // 检查文件是否存在
        struct stat file_info;
        if (stat(filename.c_str(), &file_info) == 0) {
            offset = file_info.st_size;
            resume = true;
            fp = fopen(filename.c_str(), "ab");
        } else {
            fp = fopen(filename.c_str(), "wb");
        }

        if (!fp) {
            std::cerr << "Failed to open file: " << filename << std::endl;
            return false;
        }

        // 设置CURL选项
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);
        curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
        curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, progressCallback);
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1L);

        // 如果是断点续传，设置断点位置
        if (resume && offset > 0) {
            curl_easy_setopt(curl, CURLOPT_RESUME_FROM_LARGE, offset);
            std::cout << "Resuming download from: " << offset << " bytes" << std::endl;
        }

        // 执行下载
        CURLcode res = curl_easy_perform(curl);

        if (res != CURLE_OK) {
            std::cerr << "\nDownload failed: " << curl_easy_strerror(res) << std::endl;
            return false;
        }

        std::cout << "\nDownload completed successfully!" << std::endl;
        return true;
    }
};

int main(int argc, char* argv[]) {
    if (argc != 3) {
        std::cout << "Usage: " << argv[0] << " <URL> <filename>" << std::endl;
        return 1;
    }

    std::string url = argv[1];
    std::string filename = argv[2];

    HttpDownloader downloader(url, filename);
    
    try {
        if (!downloader.download()) {
            std::cerr << "Download failed!" << std::endl;
            return 1;
        }
    } catch (const std::exception& e) {
        std::cerr << "Exception occurred: " << e.what() << std::endl;
        return 1;
    }

    return 0;
} 
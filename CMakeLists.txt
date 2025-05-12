cmake_minimum_required(VERSION 3.10)
project(http_downloader)

# 设置C++标准
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 查找CURL包
find_package(CURL REQUIRED)

# 添加可执行文件
add_executable(http_downloader http_downloader.cpp)

# 链接CURL库
target_link_libraries(http_downloader PRIVATE ${CURL_LIBRARIES})
target_include_directories(http_downloader PRIVATE ${CURL_INCLUDE_DIRS})

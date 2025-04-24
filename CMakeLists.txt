cmake_minimum_required(VERSION 3.10)
project(Downloader)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(CURL REQUIRED)
find_package(fmt REQUIRED)  

add_executable(downloader main.cpp)

target_link_libraries(downloader ${CURL_LIBRARIES})
target_link_libraries(downloader fmt::fmt)
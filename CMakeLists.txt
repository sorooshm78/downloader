cmake_minimum_required(VERSION 3.10)
project(Downloader)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(CURL REQUIRED)

add_executable(download main.cpp)

target_link_libraries(download ${CURL_LIBRARIES})
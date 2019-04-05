cmake_minimum_required(VERSION 2.8.12)

project(test_ffmpeg)

find_package(PkgConfig)

pkg_search_module(AVCODEC libavcodec)

set(CMAKE_BUILD_TYPE "Debug")
set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOMOC ON)


include_directories(
    ${AVCODEC_INCLUDE_DIRS}
    )

link_directories(
    ${AVCODEC_LIBRARY_DIRS}
    )

add_executable(${PROJECT_NAME} "main.cpp")

target_link_libraries(${PROJECT_NAME}
    avcodec
    avutil
    pthread
    )

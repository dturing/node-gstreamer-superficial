cmake_minimum_required(VERSION 2.8.4)
project(gstreamer-superficial)

find_package(PkgConfig)
pkg_check_modules(gstreamer-1.0 gstreamer-app-1.0 gstreamer-video-1.0)

include_directories(
    /usr/local/include/node/
    ./node_modules/nan
    ${gstreamer-1.0_INCLUDE_DIRS}
    ${gstreamer-app-1.0_INCLUDE_DIRS}
    ${gstreamer-video-1.0_INCLUDE_DIRS}
)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

set(SOURCE_FILES gstreamer.cpp)
add_executable(gstreamer-superficial ${SOURCE_FILES})
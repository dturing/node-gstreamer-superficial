cmake_minimum_required(VERSION 2.8.4)
project(gstreamer-superficial)

if (NOT DEFINED ENV{NODE_INCLUDE_DIRS})
    set(NODE_INCLUDE_DIRS /usr/include/node)
else()
    set(NODE_INCLUDE_DIRS $ENV{NODE_INCLUDE_DIRS})
endif()

find_package(PkgConfig)
pkg_check_modules(gstreamer-1.0 gstreamer-app-1.0 gstreamer-video-1.0)

include_directories(
    ${NODE_INCLUDE_DIRS}
    ./node_modules/nan
    ${GST_INCLUDE_DIRS}
    ${gstreamer-1.0_INCLUDE_DIRS}
)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

set(SOURCE_FILES gstreamer.cpp gstreamer.cpp GLibHelpers.cpp GObjectWrap.cpp Pipeline.cpp)
add_executable(gstreamer-superficial ${SOURCE_FILES})
target_link_libraries(gstreamer-superficial ${GST_LIBRARIES})

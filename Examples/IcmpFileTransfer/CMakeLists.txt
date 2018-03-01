cmake_minimum_required(VERSION 3.5)
project(IcmpFileTransfer)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

set(SOURCE_FILES
    Common.cpp
    Common.h
    IcmpFileTransfer-catcher.cpp
    IcmpFileTransfer-pitcher.cpp)

add_executable(IcmpFileTransfer ${SOURCE_FILES})
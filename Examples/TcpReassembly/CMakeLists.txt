cmake_minimum_required(VERSION 3.5)
project(TcpReassembly)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

set(SOURCE_FILES
    Bin/TcpReassembly
    Obj/main.d
    Obj/main.o
    main.cpp
    Makefile
    README.md)

add_executable(TcpReassembly ${SOURCE_FILES})
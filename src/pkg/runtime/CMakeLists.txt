cmake_minimum_required(VERSION 3.13)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_OSX_SYSROOT "/Library/Developer/CommandLineTools/SDKs/MacOSX14.4.sdk")
set(CMAKE_C_COMPILER "/usr/local/Cellar/gcc/14.1.0/bin/gcc-14")
set(CMAKE_CXX_COMPILER "/usr/local/Cellar/gcc/14.1.0/bin/gcc-14")

project(golang_runtime)

file(GLOB golang_runtime_src
        "*.c"
        "*/*.c"
)

add_library(golang_runtime_lib STATIC ${golang_runtime_src})

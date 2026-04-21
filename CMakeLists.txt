cmake_minimum_required(VERSION 3.10)
project(BookstoreOJ)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Optimize for OJ
add_compile_options(-O2 -pipe -s -static-libstdc++)

add_executable(code code.cpp)

cmake_minimum_required(VERSION 3.20)

project(non-blocking-cache)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(main
    src/Cache.cpp
    src/Interconnect.cpp
    src/MemoryController.cpp
    tests/main.cpp
)

target_include_directories(main PRIVATE
    include
)
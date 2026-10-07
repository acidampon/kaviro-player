cmake_minimum_required(VERSION 3.24)
project(kaviro_android LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(kaviro_android SHARED native-lib.cpp)

find_library(log-lib log)

target_link_libraries(kaviro_android PRIVATE ${log-lib})

cmake_minimum_required(VERSION 3.24)
project(cfossil C)

set(CMAKE_C_STANDARD 11)

add_library(cfossil fossil.c)

add_executable(example_basic examples/basic.c)
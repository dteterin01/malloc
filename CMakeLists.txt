cmake_minimum_required(VERSION 3.22)
project(malloc C)

set(CMAKE_C_STANDARD 17)

set(
        SOURCE malloc.c
)

add_library(test_lib ${SOURCE})

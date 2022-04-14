cmake_minimum_required(VERSION 3.17)
project(OS_ex_2)

set(CMAKE_CXX_STANDARD 11)

add_executable(OS_ex_2 main.cpp uthreads.h uthreads.cpp thread.cpp thread.h)
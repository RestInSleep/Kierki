cmake_minimum_required(VERSION 3.25)
project(Kierki)

set(CMAKE_CXX_STANDARD 20)

add_executable(kierki-serwer kierki-serwer.cpp
        err.cpp common.cpp)

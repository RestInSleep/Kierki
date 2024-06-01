cmake_minimum_required(VERSION 3.25)
project(Kierki)

set(CMAKE_CXX_STANDARD 20)

find_package(Boost 1.74.0 COMPONENTS program_options filesystem REQUIRED)
include_directories(${Boost_INCLUDE_DIRS})

add_executable(kierki-serwer kierki-serwer.cpp
        err.cpp common.cpp
        player.cpp
        cards.cpp
        )

add_executable(kierki-klient kierki-klient.cpp
        err.cpp common.cpp
        player.cpp
        cards.cpp
        )

target_link_libraries(kierki-serwer  ${Boost_LIBRARIES})
target_link_libraries(kierki-klient  ${Boost_LIBRARIES})
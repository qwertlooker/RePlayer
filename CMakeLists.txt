cmake_minimum_required(VERSION 3.16)
project(RePlayer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(RePlayer WIN32
    src/main.cpp
)

target_compile_definitions(RePlayer PRIVATE UNICODE _UNICODE)

if (MSVC)
    target_compile_options(RePlayer PRIVATE /W4 /permissive-)
else()
    target_compile_options(RePlayer PRIVATE -Wall -Wextra -Wpedantic)
endif()

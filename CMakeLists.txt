cmake_minimum_required(VERSION 3.20)
project(high_hive)

set(CMAKE_CXX_STANDARD 20)

include_directories(cpp/src)
include_directories(cpp/src/headers)

add_executable(high_hive
        cpp/src/headers/board.h
        cpp/src/headers/coords.h
        cpp/src/headers/engine.h
        cpp/src/headers/generator.h
        cpp/src/headers/moves.h
        cpp/src/headers/pieces.h
        cpp/src/headers/rules.h
        cpp/src/headers/state.h
        cpp/src/headers/uhp.h
        cpp/src/headers/utils.h
        cpp/src/board.cpp
        cpp/src/engine.cpp
        cpp/src/generator.cpp
        cpp/src/main.cpp
        cpp/src/moves.cpp
        cpp/src/rules.cpp
        cpp/src/state.cpp
        cpp/src/uhp.cpp
        cpp/src/utils.cpp
)

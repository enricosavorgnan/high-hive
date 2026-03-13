cmake_minimum_required(VERSION 3.20)
project(uhp_cpp LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(uhp
  src/main.cpp
)

target_include_directories(uhp PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/include
)

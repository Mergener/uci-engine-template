cmake_minimum_required(VERSION 3.10)
project(your_chess_engine)
set(CMAKE_CXX_STANDARD 17)

# External dependencies.
add_subdirectory(ext)

# Engine code here.
add_subdirectory(src)
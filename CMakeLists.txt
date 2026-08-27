cmake_minimum_required(VERSION 3.20)
project(pi_image_codec CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()

add_compile_options(-Wall -Wextra)

# --- bitstream ---
add_library(bitstream src/bitstream.cpp)
target_include_directories(bitstream PUBLIC src)

add_executable(bitstream_test tests/bitstream_test.cpp)
target_link_libraries(bitstream_test PRIVATE bitstream)

# --- huffman ---
add_library(huffman src/huffman.cpp)
target_link_libraries(huffman PUBLIC bitstream)
target_include_directories(huffman PUBLIC src)

add_executable(huffman_test tests/huffman_test.cpp)
target_link_libraries(huffman_test PRIVATE huffman)

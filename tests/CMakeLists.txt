cmake_minimum_required(VERSION 3.10)
project(humanus_tests)

add_executable(test_bpe test_bpe.cpp)

target_link_libraries(test_bpe PRIVATE humanus)

target_include_directories(test_bpe PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/..)
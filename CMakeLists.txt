cmake_minimum_required(VERSION 3.10)
project(humanus.cpp VERSION 0.1.0)

set(CMAKE_WARN_UNUSED_CLI YES)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if (NOT XCODE AND NOT MSVC AND NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release" "MinSizeRel" "RelWithDebInfo")
endif()

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

option(BUILD_SHARED_LIBS "build shared libraries" ${BUILD_SHARED_LIBS_DEFAULT})

if (WIN32)
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
endif()

if (MSVC)
    add_compile_options("$<$<COMPILE_LANGUAGE:C>:/utf-8>")
    add_compile_options("$<$<COMPILE_LANGUAGE:CXX>:/utf-8>")
    add_compile_options("$<$<COMPILE_LANGUAGE:C>:/bigobj>")
    add_compile_options("$<$<COMPILE_LANGUAGE:CXX>:/bigobj>")
    add_compile_options(/wd4244 /wd4267) # possible loss of data
endif()

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(OpenSSL 3.0.0 REQUIRED)
if(OPENSSL_FOUND)
    message(STATUS "OpenSSL found: ${OPENSSL_VERSION}")
    message(STATUS "OpenSSL include directory: ${OPENSSL_INCLUDE_DIR}")
    message(STATUS "OpenSSL libraries: ${OPENSSL_LIBRARIES}")
    include_directories(${OPENSSL_INCLUDE_DIR})
    add_compile_definitions(CPPHTTPLIB_OPENSSL_SUPPORT)
else()
    message(FATAL_ERROR "OpenSSL not found. Please install OpenSSL development libraries.")
endif()

# mcp
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/mcp)

# server
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/server)

# tokenizer
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/tokenizer)

# tests
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/tests)

# include
include_directories(${CMAKE_CURRENT_SOURCE_DIR})
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/common)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/mcp/include)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/mcp/common)

find_package(Threads REQUIRED)

file(GLOB BASIC_SOURCES
    "src/*.cpp"
    "src/*.cc"
)

file(GLOB AGENT_SOURCES 
    "agent/*.cpp"
    "agent/*.cc"
)

file(GLOB TOOL_SOURCES 
    "tool/*.cpp"
    "tool/*.cc"
)

file(GLOB FLOW_SOURCES 
    "flow/*.cpp"
    "flow/*.cc"
)

file(GLOB MEMORY_SOURCES
    "memory/*.cpp"
    "memory/*.cc"
    "memory/*/*.cpp"
    "memory/*/*.cc"
)

file(GLOB TOKENIZER_SOURCES
    "tokenizer/*.cpp"
    "tokenizer/*.cc"
)

# humanus core
add_library(humanus
    ${BASIC_SOURCES}
    ${AGENT_SOURCES}
    ${TOOL_SOURCES}
    ${FLOW_SOURCES}
    ${MEMORY_SOURCES}
    ${TOKENIZER_SOURCES}
)

target_link_libraries(humanus PUBLIC Threads::Threads mcp ${OPENSSL_LIBRARIES})

# examples
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/examples)
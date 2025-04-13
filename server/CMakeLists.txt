cmake_minimum_required(VERSION 3.10)

find_package(Threads REQUIRED)

set(SERVER_SOURCES
    python_execute.cpp
)

add_library(server STATIC ${SERVER_SOURCES})

target_link_libraries(server PRIVATE mcp)

find_package(Python3 COMPONENTS Development)
if(Python3_FOUND)
    message(STATUS "Python3 found: ${Python3_VERSION}")
    message(STATUS "Python3 include directory: ${Python3_INCLUDE_DIRS}")
    message(STATUS "Python3 libraries: ${Python3_LIBRARIES}")
    include_directories(${Python3_INCLUDE_DIRS})
    add_compile_definitions(PYTHON_FOUND)
else()
    message(WARNING "Python3 development libraries not found. Python interpreter will not be available.")
endif()

target_include_directories(server PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../
    ${CMAKE_CURRENT_SOURCE_DIR}/../mcp/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../mcp/common
)

add_executable(mcp_server mcp_tool_server.cpp)
target_link_libraries(mcp_server PRIVATE server mcp Threads::Threads ${OPENSSL_LIBRARIES})
target_include_directories(mcp_server PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../
    ${CMAKE_CURRENT_SOURCE_DIR}/../mcp/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../mcp/common
)
if(Python3_FOUND)
    target_link_libraries(mcp_server PRIVATE ${Python3_LIBRARIES})
endif()
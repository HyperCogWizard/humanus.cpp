set(target humanus_server)

add_executable(${target} server.cpp)

target_link_libraries(${target} PRIVATE humanus)
set(target humanus_cli)

add_executable(${target} main.cpp)

target_link_libraries(${target} PRIVATE humanus)
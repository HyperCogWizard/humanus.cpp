set(target humanus_chat)

add_executable(${target} humanus_chat.cpp)

target_link_libraries(${target} PRIVATE humanus)
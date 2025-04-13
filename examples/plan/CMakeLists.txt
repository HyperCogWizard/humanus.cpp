set(target humanus_cli_plan)

add_executable(${target} humanus_plan.cpp)

target_link_libraries(${target} PRIVATE humanus)
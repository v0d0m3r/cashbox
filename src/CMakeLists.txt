add_subdirectory(sample_library)
add_subdirectory(ftxui_sample)
add_subdirectory(library)

add_executable(project_atm main.cpp)
add_executable(project::project_atm ALIAS project_atm)

set_target_properties(project_atm PROPERTIES OUTPUT_NAME atm)
target_link_libraries(project_atm INTERFACE project_core)

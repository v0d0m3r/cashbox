add_subdirectory(sample_library)
add_subdirectory(ftxui_sample)
add_subdirectory(library)
add_subdirectory(atm)


add_executable(cashbox_logger_demo main.cpp)
add_executable(cashbox::cashbox_logger_demo ALIAS cashbox_logger_demo)

set_target_properties(cashbox_logger_demo PROPERTIES OUTPUT_NAME logger_demo)
target_link_libraries(cashbox_logger_demo INTERFACE cashbox_core)

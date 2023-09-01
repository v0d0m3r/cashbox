add_executable(cashbox_atm
    Messages.hpp Atm_machine.hpp Bank_machine.hpp Interface_machine.hpp
    main.cpp)
add_executable(cashbox::cashbox_atm ALIAS cashbox_atm)

set_target_properties(cashbox_atm PROPERTIES OUTPUT_NAME atm_app)
target_link_libraries(cashbox_atm INTERFACE cashbox_core)

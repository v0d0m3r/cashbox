#-------------------------------------------------------------------------------

add_library(cashbox_Threads INTERFACE)
add_library(cashbox::cashbox_Threads ALIAS cashbox_Threads)

#-------------------------------------------------------------------------------

find_package(Threads REQUIRED)

#-------------------------------------------------------------------------------

target_link_libraries(cashbox_Threads INTERFACE Threads::Threads)


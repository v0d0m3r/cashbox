add_library(cashbox_core INTERFACE Logger_wrap.hpp Messaging.hpp)
add_library(cashbox::cashbox_core ALIAS cashbox_core)


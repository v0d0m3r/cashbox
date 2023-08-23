add_library(project_core INTERFACE Logger_wrap.hpp)
add_library(project::project_core ALIAS project_core)


include(cmake/SystemLink.cmake)
include(cmake/LibFuzzer.cmake)
include(CMakeDependentOption)
include(CheckCXXCompilerFlag)


macro(cashbox_supports_sanitizers)
  if((CMAKE_CXX_COMPILER_ID MATCHES ".*Clang.*" OR CMAKE_CXX_COMPILER_ID MATCHES ".*GNU.*") AND NOT WIN32)
    set(SUPPORTS_UBSAN ON)
  else()
    set(SUPPORTS_UBSAN OFF)
  endif()

  if((CMAKE_CXX_COMPILER_ID MATCHES ".*Clang.*" OR CMAKE_CXX_COMPILER_ID MATCHES ".*GNU.*") AND WIN32)
    set(SUPPORTS_ASAN OFF)
  else()
    set(SUPPORTS_ASAN ON)
  endif()
endmacro()

macro(cashbox_setup_options)
  option(cashbox_ENABLE_HARDENING "Enable hardening" ON)
  option(cashbox_ENABLE_COVERAGE "Enable coverage reporting" OFF)
  cmake_dependent_option(
    cashbox_ENABLE_GLOBAL_HARDENING
    "Attempt to push hardening options to built dependencies"
    ON
    cashbox_ENABLE_HARDENING
    OFF)

  cashbox_supports_sanitizers()

  if(NOT PROJECT_IS_TOP_LEVEL OR cashbox_PACKAGING_MAINTAINER_MODE)
    option(cashbox_ENABLE_IPO "Enable IPO/LTO" OFF)
    option(cashbox_WARNINGS_AS_ERRORS "Treat Warnings As Errors" OFF)
    option(cashbox_ENABLE_USER_LINKER "Enable user-selected linker" OFF)
    option(cashbox_ENABLE_SANITIZER_ADDRESS "Enable address sanitizer" OFF)
    option(cashbox_ENABLE_SANITIZER_LEAK "Enable leak sanitizer" OFF)
    option(cashbox_ENABLE_SANITIZER_UNDEFINED "Enable undefined sanitizer" OFF)
    option(cashbox_ENABLE_SANITIZER_THREAD "Enable thread sanitizer" OFF)
    option(cashbox_ENABLE_SANITIZER_MEMORY "Enable memory sanitizer" OFF)
    option(cashbox_ENABLE_UNITY_BUILD "Enable unity builds" OFF)
    option(cashbox_ENABLE_CLANG_TIDY "Enable clang-tidy" ON)
    option(cashbox_ENABLE_CPPCHECK "Enable cpp-check analysis" OFF)
    option(cashbox_ENABLE_PCH "Enable precompiled headers" OFF)
    option(cashbox_ENABLE_CACHE "Enable ccache" OFF)
  else()
    option(cashbox_ENABLE_IPO "Enable IPO/LTO" ON)
    option(cashbox_WARNINGS_AS_ERRORS "Treat Warnings As Errors" ON)
    option(cashbox_ENABLE_USER_LINKER "Enable user-selected linker" OFF)
    option(cashbox_ENABLE_SANITIZER_ADDRESS "Enable address sanitizer" ${SUPPORTS_ASAN})
    option(cashbox_ENABLE_SANITIZER_LEAK "Enable leak sanitizer" OFF)
    option(cashbox_ENABLE_SANITIZER_UNDEFINED "Enable undefined sanitizer" ${SUPPORTS_UBSAN})
    option(cashbox_ENABLE_SANITIZER_THREAD "Enable thread sanitizer" OFF)
    option(cashbox_ENABLE_SANITIZER_MEMORY "Enable memory sanitizer" OFF)
    option(cashbox_ENABLE_UNITY_BUILD "Enable unity builds" OFF)
    option(cashbox_ENABLE_CLANG_TIDY "Enable clang-tidy" OFF)
    option(cashbox_ENABLE_CPPCHECK "Enable cpp-check analysis" ON)
    option(cashbox_ENABLE_PCH "Enable precompiled headers" OFF)
    option(cashbox_ENABLE_CACHE "Enable ccache" ON)
  endif()

  if(NOT PROJECT_IS_TOP_LEVEL)
    mark_as_advanced(
      cashbox_ENABLE_IPO
      cashbox_WARNINGS_AS_ERRORS
      cashbox_ENABLE_USER_LINKER
      cashbox_ENABLE_SANITIZER_ADDRESS
      cashbox_ENABLE_SANITIZER_LEAK
      cashbox_ENABLE_SANITIZER_UNDEFINED
      cashbox_ENABLE_SANITIZER_THREAD
      cashbox_ENABLE_SANITIZER_MEMORY
      cashbox_ENABLE_UNITY_BUILD
      cashbox_ENABLE_CLANG_TIDY
      cashbox_ENABLE_CPPCHECK
      cashbox_ENABLE_COVERAGE
      cashbox_ENABLE_PCH
      cashbox_ENABLE_CACHE)
  endif()

  cashbox_check_libfuzzer_support(LIBFUZZER_SUPPORTED)
  if(LIBFUZZER_SUPPORTED AND (cashbox_ENABLE_SANITIZER_ADDRESS OR cashbox_ENABLE_SANITIZER_THREAD OR cashbox_ENABLE_SANITIZER_UNDEFINED))
    set(DEFAULT_FUZZER ON)
  else()
    set(DEFAULT_FUZZER OFF)
  endif()

  option(cashbox_BUILD_FUZZ_TESTS "Enable fuzz testing executable" ${DEFAULT_FUZZER})

endmacro()

macro(cashbox_global_options)
  if(cashbox_ENABLE_IPO)
    include(cmake/InterproceduralOptimization.cmake)
    cashbox_enable_ipo()
  endif()

  cashbox_supports_sanitizers()

  if(cashbox_ENABLE_HARDENING AND cashbox_ENABLE_GLOBAL_HARDENING)
    include(cmake/Hardening.cmake)
    if(NOT SUPPORTS_UBSAN 
       OR cashbox_ENABLE_SANITIZER_UNDEFINED
       OR cashbox_ENABLE_SANITIZER_ADDRESS
       OR cashbox_ENABLE_SANITIZER_THREAD
       OR cashbox_ENABLE_SANITIZER_LEAK)
      set(ENABLE_UBSAN_MINIMAL_RUNTIME FALSE)
    else()
      set(ENABLE_UBSAN_MINIMAL_RUNTIME TRUE)
    endif()
    message("${cashbox_ENABLE_HARDENING} ${ENABLE_UBSAN_MINIMAL_RUNTIME} ${cashbox_ENABLE_SANITIZER_UNDEFINED}")
    cashbox_enable_hardening(cashbox_options ON ${ENABLE_UBSAN_MINIMAL_RUNTIME})
  endif()
endmacro()

macro(cashbox_local_options)
  if(PROJECT_IS_TOP_LEVEL)
    include(cmake/StandardProjectSettings.cmake)
  endif()

  add_library(cashbox_warnings INTERFACE)
  add_library(cashbox_options INTERFACE)

  include(cmake/CompilerWarnings.cmake)
  cashbox_set_project_warnings(
    cashbox_warnings
    ${cashbox_WARNINGS_AS_ERRORS}
    ""
    ""
    ""
    "")

  if(cashbox_ENABLE_USER_LINKER)
    include(cmake/Linker.cmake)
    configure_linker(cashbox_options)
  endif()

  include(cmake/Sanitizers.cmake)
  cashbox_enable_sanitizers(
    cashbox_options
    ${cashbox_ENABLE_SANITIZER_ADDRESS}
    ${cashbox_ENABLE_SANITIZER_LEAK}
    ${cashbox_ENABLE_SANITIZER_UNDEFINED}
    ${cashbox_ENABLE_SANITIZER_THREAD}
    ${cashbox_ENABLE_SANITIZER_MEMORY})

  set_target_properties(cashbox_options PROPERTIES UNITY_BUILD ${cashbox_ENABLE_UNITY_BUILD})

  if(cashbox_ENABLE_PCH)
    target_precompile_headers(
      cashbox_options
      INTERFACE
      <vector>
      <string>
      <utility>)
  endif()

  if(cashbox_ENABLE_CACHE)
    include(cmake/Cache.cmake)
    cashbox_enable_cache()
  endif()

  include(cmake/StaticAnalyzers.cmake)
  if(cashbox_ENABLE_CLANG_TIDY)
    cashbox_enable_clang_tidy(cashbox_options ${cashbox_WARNINGS_AS_ERRORS})
  endif()

  if(cashbox_ENABLE_CPPCHECK)
    cashbox_enable_cppcheck(${cashbox_WARNINGS_AS_ERRORS} "" # override cppcheck options
    )
  endif()

  if(cashbox_ENABLE_COVERAGE)
    include(cmake/Tests.cmake)
    cashbox_enable_coverage(cashbox_options)
  endif()

  if(cashbox_WARNINGS_AS_ERRORS)
    check_cxx_compiler_flag("-Wl,--fatal-warnings" LINKER_FATAL_WARNINGS)
    if(LINKER_FATAL_WARNINGS)
      # This is not working consistently, so disabling for now
      # target_link_options(cashbox_options INTERFACE -Wl,--fatal-warnings)
    endif()
  endif()

  if(cashbox_ENABLE_HARDENING AND NOT cashbox_ENABLE_GLOBAL_HARDENING)
    include(cmake/Hardening.cmake)
    if(NOT SUPPORTS_UBSAN 
       OR cashbox_ENABLE_SANITIZER_UNDEFINED
       OR cashbox_ENABLE_SANITIZER_ADDRESS
       OR cashbox_ENABLE_SANITIZER_THREAD
       OR cashbox_ENABLE_SANITIZER_LEAK)
      set(ENABLE_UBSAN_MINIMAL_RUNTIME FALSE)
    else()
      set(ENABLE_UBSAN_MINIMAL_RUNTIME TRUE)
    endif()
    cashbox_enable_hardening(cashbox_options OFF ${ENABLE_UBSAN_MINIMAL_RUNTIME})
  endif()

endmacro()

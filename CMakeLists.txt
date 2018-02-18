
cmake_minimum_required(VERSION 3.4)
project(amp)


list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")
list(APPEND CMAKE_PREFIX_PATH "${CMAKE_SOURCE_DIR}/contrib")
list(APPEND CMAKE_PREFIX_PATH "${CMAKE_SOURCE_DIR}/../Qt/5.9.1/clang_64")


option(AMP_ENABLE_LTO "Enable link time optimization" OFF)
option(AMP_ENABLE_RTTI "Enable run time type information" OFF)
option(AMP_ENABLE_WERROR "Treat compiler warnings as errors" OFF)


add_library(amp_runtime INTERFACE)

target_compile_features(amp_runtime INTERFACE
    cxx_std_17
    cxx_alias_templates
    cxx_auto_type
    cxx_constexpr
    cxx_decltype
    cxx_decltype_auto
    cxx_defaulted_functions
    cxx_delegating_constructors
    cxx_deleted_functions
    cxx_enum_forward_declarations
    cxx_explicit_conversions
    cxx_final
    cxx_generic_lambdas
    cxx_inline_namespaces
    cxx_lambdas
    cxx_lambda_init_captures
    cxx_noexcept
    cxx_nullptr
    cxx_override
    cxx_range_for
    cxx_reference_qualified_functions
    cxx_relaxed_constexpr
    cxx_return_type_deduction
    cxx_rvalue_references
    cxx_static_assert
    cxx_trailing_return_types
    cxx_strong_enums
    cxx_uniform_initialization
    cxx_user_literals
    cxx_variable_templates
    cxx_variadic_macros)

target_include_directories(amp_runtime INTERFACE include)
target_include_directories(amp_runtime SYSTEM INTERFACE contrib/include)


include(DetermineGCCCompatible)

if(AMP_COMPILER_IS_GCC_COMPATIBLE)
    if("${CMAKE_CXX_COMPILER_ID}" MATCHES "Clang")
        target_compile_options(amp_runtime INTERFACE
            "-fvisibility=hidden"
            "-Weverything"
            "-Wno-padded"
            "-Wno-c++98-compat-pedantic"
            "-Wno-weak-vtables"
            "-Wno-used-but-marked-unused"
            "-Wno-exit-time-destructors"
            "-Wno-global-constructors"
            "-Wno-disabled-macro-expansion")
    else()
        target_compile_options(amp_runtime INTERFACE
            "-fvisibility=hidden"
            "-Wall"
            "-Wextra")
    endif()
endif()

if(AMP_ENABLE_LTO)
    if(AMP_COMPILER_IS_GCC_COMPATIBLE)
        target_compile_options(amp_runtime INTERFACE "-flto")
    elseif(MSVC)
        target_compile_options(amp_runtime INTERFACE "/GL")
    endif()
endif()

if(NOT AMP_ENABLE_RTTI)
    if(AMP_COMPILER_IS_GCC_COMPATIBLE)
        target_compile_options(amp_runtime INTERFACE "-fno-rtti")
    elseif(MSVC)
        target_compile_options(amp_runtime INTERFACE "/GR-")
    endif()
endif()

if(AMP_ENABLE_WERROR)
    if(AMP_COMPILER_IS_GCC_COMPATIBLE)
        target_compile_options(amp_runtime INTERFACE "-Werror")
    elseif(MSVC)
        target_compile_options(amp_runtime INTERFACE "/WX")
    endif()
endif()

add_library(AMP::Runtime ALIAS amp_runtime)


enable_testing()
add_subdirectory(src)
add_subdirectory(plugins)
add_subdirectory(tests EXCLUDE_FROM_ALL)


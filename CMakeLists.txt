cmake_minimum_required(VERSION 3.28...4.4)

###############################################################################
## Reference projects
###############################################################################

# The reference presets build one of the projects under reference/, which adds micras-lib back the
# way any consumer does. They are selected here because a preset always configures this directory.
if(MICRAS_LIB_REFERENCE AND NOT MICRAS_LIB_REFERENCE_ADDED)
    set(MICRAS_LIB_REFERENCE_ADDED TRUE)
    add_subdirectory(reference/${MICRAS_LIB_REFERENCE})
    return()
endif()

###############################################################################
## Project
###############################################################################

# A project that reaches micras-lib through two submodule paths uses the one its top level adds
# first; every later add_subdirectory is a no-op
get_property(MICRAS_LIB_ALREADY_ADDED GLOBAL PROPERTY MICRAS_LIB_ADDED)

if(MICRAS_LIB_ALREADY_ADDED)
    return()
endif()

set_property(GLOBAL PROPERTY MICRAS_LIB_ADDED TRUE)

project(micras_lib LANGUAGES C CXX)

# As a subproject, the consumer's language settings and flags reach the libraries by inheritance
if(micras_lib_IS_TOP_LEVEL)
    set(CMAKE_C_STANDARD 17)
    set(CMAKE_C_STANDARD_REQUIRED ON)
    set(CMAKE_C_EXTENSIONS ON)

    set(CMAKE_CXX_STANDARD 23)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    set(CMAKE_CXX_EXTENSIONS OFF)

    set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

    # No target declares a module, and the scan puts -fmodules-ts and a module mapper into the
    # compilation database, which makes clang-tidy reject every file
    set(CMAKE_CXX_SCAN_FOR_MODULES OFF)

    if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
        set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
    endif()
endif()

###############################################################################
## Options
###############################################################################

include(CMakeDependentOption)

option(MICRAS_LIB_NAV "Add micras_nav" ON)
option(MICRAS_LIB_COMM "Add micras_comm" ON)
option(MICRAS_LIB_HAL "Add micras_hal" ON)
cmake_dependent_option(MICRAS_LIB_PROXY "Add micras_proxy, which fetches lsm6dsv-pid" ON "MICRAS_LIB_HAL" OFF)

set(MICRAS_LIB_HAL_BACKEND "stm32" CACHE STRING "Backend of micras_hal: stm32 or host")
set_property(CACHE MICRAS_LIB_HAL_BACKEND PROPERTY STRINGS stm32 host)

set(MICRAS_LIB_STM32_FAMILY "" CACHE STRING "STM32 family of the stm32 backend, detected from the board target when empty")
set_property(CACHE MICRAS_LIB_STM32_FAMILY PROPERTY STRINGS "" h7)

set(MICRAS_LIB_CUBE_TARGET "stm32cubemx" CACHE STRING "Board target micras_hal links, on every backend")

option(MICRAS_LIB_WARNINGS "Apply the project warnings to every library target" ON)
option(MICRAS_LIB_WERROR "Treat the warnings of the library targets as errors" OFF)

option(MICRAS_LIB_TESTS "Build the tests, on the host backend; fetches doctest" ${micras_lib_IS_TOP_LEVEL})
option(MICRAS_LIB_DOCS "Add the docs target" ${micras_lib_IS_TOP_LEVEL})

if(NOT MICRAS_LIB_HAL_BACKEND MATCHES "^(stm32|host)$")
    message(FATAL_ERROR "micras-lib: MICRAS_LIB_HAL_BACKEND is '${MICRAS_LIB_HAL_BACKEND}'; it must be stm32 or host.")
endif()

if(MICRAS_LIB_TESTS AND MICRAS_LIB_HAL AND
   NOT (MICRAS_LIB_HAL_BACKEND STREQUAL "host" AND MICRAS_LIB_CUBE_TARGET STREQUAL "micras_lib_host_board"))
    message(FATAL_ERROR
        "micras-lib: the tests run on the host backend, over the fake Cube layer of tests/support. "
        "Configure with the host preset (MICRAS_LIB_HAL_BACKEND=host, "
        "MICRAS_LIB_CUBE_TARGET=micras_lib_host_board), or set MICRAS_LIB_TESTS=OFF.")
endif()

include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/micras_warnings.cmake)

###############################################################################
## Libraries
###############################################################################

# Every library requires C++23 without extensions, whatever the consumer builds with
function(micras_lib_setup_library TARGET_NAME)
    target_compile_features(${TARGET_NAME} PUBLIC cxx_std_23)
    set_target_properties(${TARGET_NAME} PROPERTIES CXX_EXTENSIONS OFF)

    if(MICRAS_LIB_WARNINGS)
        micras_apply_warnings(${TARGET_NAME} WERROR ${MICRAS_LIB_WERROR})
    endif()
endfunction()

add_subdirectory(micras_core)

# The tests' board target, defined before the HAL as any consumer's
if(MICRAS_LIB_TESTS AND MICRAS_LIB_HAL)
    add_subdirectory(tests/support/host_board)
endif()

if(MICRAS_LIB_HAL)
    add_subdirectory(micras_hal)
endif()

if(MICRAS_LIB_PROXY)
    add_subdirectory(micras_proxy)
endif()

if(MICRAS_LIB_NAV)
    add_subdirectory(micras_nav)
endif()

if(MICRAS_LIB_COMM)
    add_subdirectory(micras_comm)
endif()

###############################################################################
## Tests, style and documentation
###############################################################################

if(MICRAS_LIB_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

if(micras_lib_IS_TOP_LEVEL)
    include(cmake/micras_lint.cmake)
endif()

if(MICRAS_LIB_DOCS)
    include(cmake/micras_docs.cmake)
endif()

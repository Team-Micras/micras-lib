###############################################################################
## Reference project: generated tree, micras-lib, bench program and lint
###############################################################################

# Included by reference/<family>/CMakeLists.txt after its project(), with common/toolchain.cmake read
# before it.

# The STM32CubeMX CMake generator writes its sources into ${CMAKE_PROJECT_NAME} instead of a target of
# its own, so the variable is pointed at a target created to hold them
add_library(cube_app OBJECT)
set(MICRAS_REFERENCE_PROJECT_NAME ${CMAKE_PROJECT_NAME})
set(CMAKE_PROJECT_NAME cube_app)
add_subdirectory(${MICRAS_REFERENCE_DIR}/cmake/stm32cubemx stm32cubemx)
set(CMAKE_PROJECT_NAME ${MICRAS_REFERENCE_PROJECT_NAME})

# The generated headers are not the project's to warn about
set_target_properties(stm32cubemx PROPERTIES SYSTEM TRUE)

foreach(CUBE_OBJECT_LIBRARY cube_app STM32_Drivers)
    target_compile_options(${CUBE_OBJECT_LIBRARY} PRIVATE -w)
endforeach()

###############################################################################
## micras-lib
###############################################################################

set(MICRAS_LIB_WERROR ON)
add_subdirectory(${MICRAS_REFERENCE_COMMON_DIR}/../.. micras-lib EXCLUDE_FROM_ALL)

###############################################################################
## Bench program
###############################################################################

set(MICRAS_BENCH_TARGET micras_lib_bench_${MICRAS_REFERENCE_FAMILY})

# The vector table, the interrupt handlers and the MX_*_Init functions are linked into the executable,
# which is where micras-lib's callbacks have to win over the vendor's weak ones
add_executable(${MICRAS_BENCH_TARGET}
    ${MICRAS_REFERENCE_DIR}/bench.cpp
    ${MICRAS_REFERENCE_COMMON_DIR}/bench.cpp
)

target_include_directories(${MICRAS_BENCH_TARGET} PRIVATE
    ${MICRAS_REFERENCE_COMMON_DIR}
)

target_link_libraries(${MICRAS_BENCH_TARGET} PRIVATE
    micras::proxy
    micras::nav
    micras::comm
    cube_app
    STM32_Drivers
)

micras_apply_warnings(${MICRAS_BENCH_TARGET} WERROR ON)

target_link_options(${MICRAS_BENCH_TARGET} PRIVATE -Wl,-Map=$<TARGET_FILE_BASE_NAME:${MICRAS_BENCH_TARGET}>.map)

add_custom_command(TARGET ${MICRAS_BENCH_TARGET} POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O ihex $<TARGET_FILE:${MICRAS_BENCH_TARGET}> $<TARGET_FILE_BASE_NAME:${MICRAS_BENCH_TARGET}>.hex
    COMMAND ${CMAKE_SIZE} $<TARGET_FILE:${MICRAS_BENCH_TARGET}>
)

###############################################################################
## Lint
###############################################################################

# The stm32 backend of this family and the bench are linted here, against the family's ARM headers,
# since no host build compiles them
include(${MICRAS_REFERENCE_COMMON_DIR}/../../cmake/micras_lint.cmake)

# clang finds the ARM target from the compiler's name, but not the headers of its toolchain
execute_process(
    COMMAND ${CMAKE_CXX_COMPILER} -print-search-dirs
    OUTPUT_VARIABLE MICRAS_ARM_SEARCH_DIRS
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

execute_process(
    COMMAND ${CMAKE_CXX_COMPILER} -dumpmachine
    OUTPUT_VARIABLE MICRAS_ARM_TRIPLE
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

string(REGEX MATCH "install: ([^\n]+)/" MICRAS_ARM_INSTALL_MATCH ${MICRAS_ARM_SEARCH_DIRS})
set(MICRAS_ARM_INSTALL_DIR ${CMAKE_MATCH_1})
get_filename_component(MICRAS_ARM_GCC_VERSION ${MICRAS_ARM_INSTALL_DIR} NAME)
get_filename_component(MICRAS_ARM_PREFIX "${MICRAS_ARM_INSTALL_DIR}/../../../" REALPATH)
set(MICRAS_ARM_SYSROOT ${MICRAS_ARM_PREFIX}/${MICRAS_ARM_TRIPLE})
set(MICRAS_ARM_CXX_INCLUDE_DIR ${MICRAS_ARM_SYSROOT}/include/c++/${MICRAS_ARM_GCC_VERSION})

file(GLOB MICRAS_REFERENCE_LINT_SOURCES CONFIGURE_DEPENDS
    ${MICRAS_LIB_ROOT}/micras_hal/stm32/src/*.cpp
    ${MICRAS_LIB_ROOT}/micras_hal/stm32/${MICRAS_REFERENCE_FAMILY}/src/*.cpp
    ${MICRAS_LIB_ROOT}/reference/common/*.cpp
    ${MICRAS_LIB_ROOT}/reference/${MICRAS_REFERENCE_FAMILY}/bench.cpp
)

micras_add_lint_targets(
    SOURCES ${MICRAS_REFERENCE_LINT_SOURCES}
    EXTRA_ARGS
        --sysroot=${MICRAS_ARM_SYSROOT}/
        -I${MICRAS_ARM_CXX_INCLUDE_DIR}/
        -I${MICRAS_ARM_CXX_INCLUDE_DIR}/${MICRAS_ARM_TRIPLE}/
)

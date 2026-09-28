cmake_minimum_required(VERSION 3.28...4.4)

###############################################################################
## Reference STM32H725 project
###############################################################################

# The smallest CubeMX project that exercises every peripheral micras-lib uses, built and linked with
# its bench program the way a consumer of the stm32 backend builds its firmware. The Cube files are
# generated from reference.ioc (the CI generates them with .docker/Dockerfile) and never committed.

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_SCAN_FOR_MODULES OFF)

set(MICRAS_REFERENCE_DIR ${CMAKE_CURRENT_LIST_DIR})

if(NOT EXISTS ${MICRAS_REFERENCE_DIR}/cmake/stm32cubemx/CMakeLists.txt)
    message(FATAL_ERROR
        "micras-lib reference/h7: the Cube files are not generated. Generate them into reference/h7 with "
        "'docker build --file .docker/Dockerfile --target generated-h7 --output type=local,dest=reference/h7 .' "
        "from the root of micras-lib, or with STM32CubeMX from reference.ioc.")
endif()

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

###############################################################################
## Toolchain
###############################################################################

# The generated toolchain file names the linker script relative to CMAKE_SOURCE_DIR, so it is pointed
# at the generated tree while the file is read
set(MICRAS_REFERENCE_SOURCE_DIR ${CMAKE_SOURCE_DIR})
set(CMAKE_SOURCE_DIR ${MICRAS_REFERENCE_DIR})
include(${MICRAS_REFERENCE_DIR}/cmake/gcc-arm-none-eabi.cmake)
set(CMAKE_SOURCE_DIR ${MICRAS_REFERENCE_SOURCE_DIR})

# One map file per target instead of the one name the generated file gives every target
string(REGEX REPLACE " *-Wl,-Map=[^ ]*" "" CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS}")

# The flags a consumer builds its firmware with: the same as the Micras firmware's
set(MICRAS_MATH_FLAGS "-fno-math-errno -ffp-contract=fast")

set(CMAKE_C_FLAGS_DEBUG "-Og -g3")
set(CMAKE_CXX_FLAGS_DEBUG "-Og -g3")
set(CMAKE_C_FLAGS_RELEASE "-O2 -g3 ${MICRAS_MATH_FLAGS}")
set(CMAKE_CXX_FLAGS_RELEASE "-O2 -g3 ${MICRAS_MATH_FLAGS}")

# Link time optimization through the property, so that CMake archives with gcc-ar
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE)
set(CMAKE_EXE_LINKER_FLAGS_RELEASE "-O2")

project(micras_lib_reference_h7 C CXX ASM)

###############################################################################
## Generated tree
###############################################################################

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
add_subdirectory(${MICRAS_REFERENCE_DIR}/../.. micras-lib EXCLUDE_FROM_ALL)

###############################################################################
## Bench program
###############################################################################

# The vector table, the interrupt handlers and the MX_*_Init functions are linked into the executable,
# which is where micras-lib's callbacks have to win over the vendor's weak ones
add_executable(micras_lib_bench_h7
    ${MICRAS_REFERENCE_DIR}/bench.cpp
)

target_link_libraries(micras_lib_bench_h7 PRIVATE
    micras::proxy
    micras::nav
    micras::comm
    cube_app
    STM32_Drivers
)

micras_apply_warnings(micras_lib_bench_h7 WERROR ON)

target_link_options(micras_lib_bench_h7 PRIVATE -Wl,-Map=$<TARGET_FILE_BASE_NAME:micras_lib_bench_h7>.map)

add_custom_command(TARGET micras_lib_bench_h7 POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O ihex $<TARGET_FILE:micras_lib_bench_h7> $<TARGET_FILE_BASE_NAME:micras_lib_bench_h7>.hex
    COMMAND ${CMAKE_SIZE} $<TARGET_FILE:micras_lib_bench_h7>
)

###############################################################################
## Lint
###############################################################################

# The stm32 backend and the bench are linted here, against the ARM headers, since no host build
# compiles them
include(${MICRAS_REFERENCE_DIR}/../../cmake/micras_lint.cmake)

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
    ${MICRAS_LIB_ROOT}/micras_hal/stm32/h7/src/*.cpp
    ${MICRAS_LIB_ROOT}/reference/h7/bench.cpp
)

micras_add_lint_targets(
    SOURCES ${MICRAS_REFERENCE_LINT_SOURCES}
    EXTRA_ARGS
        --sysroot=${MICRAS_ARM_SYSROOT}/
        -I${MICRAS_ARM_CXX_INCLUDE_DIR}/
        -I${MICRAS_ARM_CXX_INCLUDE_DIR}/${MICRAS_ARM_TRIPLE}/
)

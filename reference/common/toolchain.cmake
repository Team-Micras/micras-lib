###############################################################################
## Reference project: language, flags and the generated toolchain file
###############################################################################

# Included by reference/<family>/CMakeLists.txt before its project(), with MICRAS_REFERENCE_FAMILY set.
# The Cube files are generated from reference/<family>/reference.ioc (the CI generates them with
# .docker/Dockerfile) and never committed.

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_SCAN_FOR_MODULES OFF)

get_filename_component(MICRAS_REFERENCE_DIR ${CMAKE_CURRENT_LIST_DIR}/../${MICRAS_REFERENCE_FAMILY} ABSOLUTE)
set(MICRAS_REFERENCE_COMMON_DIR ${CMAKE_CURRENT_LIST_DIR})

if(NOT EXISTS ${MICRAS_REFERENCE_DIR}/cmake/stm32cubemx/CMakeLists.txt)
    message(FATAL_ERROR
        "micras-lib reference/${MICRAS_REFERENCE_FAMILY}: the Cube files are not generated. Generate them into "
        "reference/${MICRAS_REFERENCE_FAMILY} with 'docker build --file .docker/Dockerfile --target "
        "generated-${MICRAS_REFERENCE_FAMILY} --output type=local,dest=reference/${MICRAS_REFERENCE_FAMILY} .' "
        "from the root of micras-lib, or with STM32CubeMX from reference.ioc.")
endif()

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

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

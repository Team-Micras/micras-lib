file(GLOB_RECURSE MICRAS_PROXY_SOURCES CONFIGURE_DEPENDS "src/*.c*")

# The one declaration of ST's register driver, pinned by the commit of v5.1.1, so that every project
# in a build shares one copy
include(FetchContent)
FetchContent_Declare(
    lsm6dsv-pid
    GIT_REPOSITORY https://github.com/STMicroelectronics/lsm6dsv-pid.git
    GIT_TAG 6ede05a3fde7eae80bc1b47affc08257191c1aba
)

FetchContent_MakeAvailable(lsm6dsv-pid)
file(GLOB_RECURSE LSM6DSV_SOURCES CONFIGURE_DEPENDS "${lsm6dsv-pid_SOURCE_DIR}/*.c")

# The vendor sources are not ours to warn about. Source properties belong to the directory that sets
# them, so this stays next to the target that compiles the sources.
set_source_files_properties(
    ${LSM6DSV_SOURCES}
    PROPERTIES
    COMPILE_FLAGS "-w"
)

add_library(micras_proxy STATIC
    ${MICRAS_PROXY_SOURCES}
    ${LSM6DSV_SOURCES}
)

add_library(micras::proxy ALIAS micras_proxy)

target_include_directories(micras_proxy PUBLIC
    include
    ${lsm6dsv-pid_SOURCE_DIR}
)

target_link_libraries(micras_proxy PUBLIC
    micras::hal
    micras::core
)

# The vendor driver is C17 with extensions, whatever the consumer compiles its own C with
set_target_properties(micras_proxy PROPERTIES
    C_STANDARD 17
    C_STANDARD_REQUIRED ON
    C_EXTENSIONS ON
)

micras_lib_setup_library(micras_proxy)

# The chip models are SPI devices of the host backend, so they exist only there
if(MICRAS_LIB_HAL_BACKEND STREQUAL "host")
    add_subdirectory(models)
endif()

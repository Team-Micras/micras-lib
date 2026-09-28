file(GLOB_RECURSE MICRAS_CORE_SOURCES CONFIGURE_DEPENDS "src/*.c*")

add_library(micras_core STATIC
    ${MICRAS_CORE_SOURCES}
)

add_library(micras::core ALIAS micras_core)

target_include_directories(micras_core PUBLIC
    include
)

micras_lib_setup_library(micras_core)

file(GLOB_RECURSE MICRAS_NAV_SOURCES CONFIGURE_DEPENDS "src/*.c*")

add_library(micras_nav STATIC
    ${MICRAS_NAV_SOURCES}
)

add_library(micras::nav ALIAS micras_nav)

target_include_directories(micras_nav PUBLIC
    include
)

# The navigation takes plain data and gives plain data back, so it links nothing that can reach the
# hardware and builds anywhere micras::core does
target_link_libraries(micras_nav PUBLIC
    micras::core
)

# A program that designs the turn tables as a constant expression needs more evaluation steps than
# clang allows by default
target_compile_options(micras_nav INTERFACE
    $<$<CXX_COMPILER_ID:Clang>:-fconstexpr-steps=1000000000>
)

micras_lib_setup_library(micras_nav)

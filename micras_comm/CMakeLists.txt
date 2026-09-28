file(GLOB_RECURSE MICRAS_COMM_SOURCES CONFIGURE_DEPENDS "src/*.c*")

add_library(micras_comm STATIC
    ${MICRAS_COMM_SOURCES}
)

add_library(micras::comm ALIAS micras_comm)

target_include_directories(micras_comm PUBLIC
    include
)

# Only the reflection and the framing, so that the whole session layer can be built and tested
# without a single peripheral behind it
target_link_libraries(micras_comm PUBLIC
    micras::core
)

micras_lib_setup_library(micras_comm)

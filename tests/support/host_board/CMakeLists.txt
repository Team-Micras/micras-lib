# The fake Cube layer of the reference projects. micras_hal links it as its board target, and it links
# micras_hal back for the host backend's headers and registry, a cycle of two static libraries that
# CMake links by repeating them.
add_library(micras_lib_host_board STATIC
    handles.cpp
)

target_include_directories(micras_lib_host_board PUBLIC
    .
)

target_link_libraries(micras_lib_host_board PUBLIC
    micras::hal
)

micras_apply_warnings(micras_lib_host_board WERROR ${MICRAS_LIB_WERROR})

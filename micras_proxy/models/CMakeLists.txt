# SPI devices for the host micras_hal: they know the SPI device slot of the host backend and nothing
# else. A consumer's bindings attach them to its buses and feed them.
add_library(micras_proxy_models STATIC
    src/as5047u_model.cpp
    src/lsm6dsv_model.cpp
)

add_library(micras::proxy_models ALIAS micras_proxy_models)

target_include_directories(micras_proxy_models PUBLIC
    include
)

target_link_libraries(micras_proxy_models PUBLIC
    micras::hal
)

micras_lib_setup_library(micras_proxy_models)

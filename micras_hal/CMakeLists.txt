# The board target carries main.h and the vendor or fake Cube layer behind it, which the headers
# include, so it has to exist before the HAL is added
if(NOT TARGET ${MICRAS_LIB_CUBE_TARGET})
    message(FATAL_ERROR
        "micras-lib: the ${MICRAS_LIB_HAL_BACKEND} HAL links '${MICRAS_LIB_CUBE_TARGET}', which is not "
        "defined yet. Define it (CubeMX's stm32cubemx, or a fake Cube layer for the host) before adding "
        "micras-lib, or set MICRAS_LIB_CUBE_TARGET.")
endif()

add_library(micras_hal STATIC)
add_library(micras::hal ALIAS micras_hal)

target_include_directories(micras_hal PUBLIC
    include
)

# stm32cubemx, or the consumer's fake Cube layer on the host, carries the vendor include directories
# and the device macros the public headers name, so it propagates
target_link_libraries(micras_hal PUBLIC
    ${MICRAS_LIB_CUBE_TARGET}
)

# The HAL uses nothing of the core. The link only keeps the core's archive after the HAL's on a
# consumer's link line, the order the robot's image was verified with.
target_link_libraries(micras_hal PRIVATE
    micras::core
)

if(MICRAS_LIB_HAL_BACKEND STREQUAL "stm32")
    # The family is read from the device macro CubeMX puts on its interface target
    if(MICRAS_LIB_STM32_FAMILY)
        set(MICRAS_HAL_FAMILY ${MICRAS_LIB_STM32_FAMILY})
    else()
        get_target_property(MICRAS_CUBE_DEFINITIONS ${MICRAS_LIB_CUBE_TARGET} INTERFACE_COMPILE_DEFINITIONS)

        if(MICRAS_CUBE_DEFINITIONS MATCHES "STM32H7")
            set(MICRAS_HAL_FAMILY h7)
        elseif(MICRAS_CUBE_DEFINITIONS MATCHES "STM32G4")
            set(MICRAS_HAL_FAMILY g4)
        else()
            message(FATAL_ERROR "micras-lib: cannot tell the STM32 family from ${MICRAS_LIB_CUBE_TARGET}; "
                                "set MICRAS_LIB_STM32_FAMILY to h7 or g4.")
        endif()
    endif()

    if(NOT IS_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/stm32/${MICRAS_HAL_FAMILY})
        message(FATAL_ERROR "micras-lib: the stm32 backend has no code for the '${MICRAS_HAL_FAMILY}' family.")
    endif()

    file(GLOB MICRAS_HAL_SOURCES CONFIGURE_DEPENDS
        stm32/src/*.cpp
        stm32/${MICRAS_HAL_FAMILY}/src/*.cpp
    )

    target_include_directories(micras_hal PUBLIC
        stm32/${MICRAS_HAL_FAMILY}/include
    )
else()
    file(GLOB MICRAS_HAL_SOURCES CONFIGURE_DEPENDS
        host/src/*.cpp
    )

    target_include_directories(micras_hal PUBLIC
        host/include
    )
endif()

target_sources(micras_hal PRIVATE
    ${MICRAS_HAL_SOURCES}
)

micras_lib_setup_library(micras_hal)

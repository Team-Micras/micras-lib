###############################################################################
## Tests, on the host backend
###############################################################################

include(FetchContent)

# Pinned by the commit of v2.5.3. SYSTEM keeps the project warnings out of its header.
FetchContent_Declare(
    doctest
    GIT_REPOSITORY https://github.com/doctest/doctest.git
    GIT_TAG 2d0a9359a60c51affe2a9bebb1be1dca47868151
    SYSTEM
)

set(DOCTEST_NO_INSTALL ON)
set(DOCTEST_WITH_TESTS OFF)
FetchContent_MakeAvailable(doctest)

include(${doctest_SOURCE_DIR}/scripts/cmake/doctest.cmake)

###############################################################################
## Support
###############################################################################

add_library(micras_lib_test_main STATIC
    support/main.cpp
)

target_link_libraries(micras_lib_test_main PUBLIC
    doctest::doctest
)

target_include_directories(micras_lib_test_main PUBLIC
    support
)

micras_apply_warnings(micras_lib_test_main WERROR ${MICRAS_LIB_WERROR})

# micras_lib_add_suite(<suite> <library>...)
#
# One executable per folder of tests, whose test cases CTest discovers one by one
function(micras_lib_add_suite SUITE)
    file(GLOB SUITE_SOURCES CONFIGURE_DEPENDS ${SUITE}/*.cpp)

    add_executable(micras_lib_${SUITE}_tests
        ${SUITE_SOURCES}
    )

    target_link_libraries(micras_lib_${SUITE}_tests PRIVATE
        micras_lib_test_main
        ${ARGN}
    )

    micras_apply_warnings(micras_lib_${SUITE}_tests WERROR ${MICRAS_LIB_WERROR})

    doctest_discover_tests(micras_lib_${SUITE}_tests
        TEST_PREFIX "${SUITE}/"
    )
endfunction()

###############################################################################
## Suites
###############################################################################

micras_lib_add_suite(core micras::core)

if(MICRAS_LIB_COMM)
    micras_lib_add_suite(comm micras::comm)
endif()

if(MICRAS_LIB_NAV)
    micras_lib_add_suite(nav micras::nav)

    target_compile_definitions(micras_lib_nav_tests PRIVATE
        MICRAS_LIB_TEST_MAZES="${CMAKE_CURRENT_SOURCE_DIR}/data/mazes"
    )
endif()

if(MICRAS_LIB_HAL)
    micras_lib_add_suite(hal micras::hal)
endif()

if(MICRAS_LIB_PROXY)
    micras_lib_add_suite(proxy micras::proxy micras::proxy_models)
    micras_lib_add_suite(models micras::proxy_models micras::proxy)
endif()

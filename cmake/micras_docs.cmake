###############################################################################
## Documentation
###############################################################################

find_program(MICRAS_DOXYGEN doxygen)

if(NOT MICRAS_DOXYGEN)
    message(FATAL_ERROR "micras-lib: doxygen was not found. Install it, or set MICRAS_LIB_DOCS=OFF.")
endif()

include(FetchContent)

# The theme the Doxyfile names through $(DOXYGEN_AWESOME_DIR), pinned by the commit of v2.5.0
FetchContent_Declare(
    doxygen-awesome-css
    GIT_REPOSITORY https://github.com/jothepro/doxygen-awesome-css.git
    GIT_TAG 46483f1e5a70ffb9ecd3b82d0a1cd1b24edf13da
)

FetchContent_MakeAvailable(doxygen-awesome-css)

# The Doxyfile reads the version, the theme and the output directory from the environment
add_custom_target(docs
    COMMAND ${CMAKE_COMMAND} -E env
            DOXYGEN_AWESOME_DIR=${doxygen-awesome-css_SOURCE_DIR}
            MICRAS_LIB_DOCS_DIR=${CMAKE_BINARY_DIR}/docs
            sh -c "MICRAS_LIB_VERSION=\"$(git describe --always --dirty --tags 2>/dev/null || echo unknown)\" exec ${MICRAS_DOXYGEN} Doxyfile"
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMENT "Generating the HTML documentation in ${CMAKE_BINARY_DIR}/docs"
    VERBATIM
)

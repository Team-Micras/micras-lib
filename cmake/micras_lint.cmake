###############################################################################
## Formatting and linting
###############################################################################

# The clang tools are found by their versioned names, so that a distribution update or another
# version first on the PATH can move neither the formatting rules nor the enabled check set
set(MICRAS_CLANG_VERSION 22)

foreach(TOOL clang-format clang-tidy run-clang-tidy clang-apply-replacements)
    string(TOUPPER ${TOOL} TOOL_VARIABLE)
    string(REPLACE "-" "_" TOOL_VARIABLE ${TOOL_VARIABLE})

    find_program(MICRAS_${TOOL_VARIABLE} ${TOOL}-${MICRAS_CLANG_VERSION})

    if(NOT MICRAS_${TOOL_VARIABLE})
        message(FATAL_ERROR
            "micras-lib: ${TOOL}-${MICRAS_CLANG_VERSION} was not found. The format and lint targets need "
            "clang ${MICRAS_CLANG_VERSION} (apt install clang-format-${MICRAS_CLANG_VERSION} "
            "clang-tidy-${MICRAS_CLANG_VERSION}), found by that name.")
    endif()
endforeach()

# micras_add_format_targets(<file>...)
#
# format rewrites the files, format_check fails on the first one that is not formatted
function(micras_add_format_targets)
    add_custom_target(format
        COMMAND ${MICRAS_CLANG_FORMAT} -style=file -i ${ARGN} --verbose
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    )

    add_custom_target(format_check
        COMMAND ${MICRAS_CLANG_FORMAT} -style=file --dry-run --Werror ${ARGN}
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    )
endfunction()

# micras_add_lint_targets(SOURCES <file>... [EXTRA_ARGS <argument>...])
#
# lint runs clang-tidy on the sources of the compilation database, with the extra arguments the
# compiler needs, and lint_fix applies the fixes it can
function(micras_add_lint_targets)
    cmake_parse_arguments(PARSE_ARGV 0 LINT "" "" "SOURCES;EXTRA_ARGS")

    list(JOIN LINT_EXTRA_ARGS "\n" MICRAS_TIDY_EXTRA_ARGS)
    set(MICRAS_CLANG_TIDY ${MICRAS_CLANG_TIDY})
    set(MICRAS_RUN_CLANG_TIDY ${MICRAS_RUN_CLANG_TIDY})
    set(MICRAS_CLANG_APPLY_REPLACEMENTS ${MICRAS_CLANG_APPLY_REPLACEMENTS})

    set(SCRIPT_PATH ${CMAKE_BINARY_DIR}/run_clang_tidy.sh)
    configure_file(${CMAKE_CURRENT_FUNCTION_LIST_DIR}/templates/run_clang_tidy.sh.in ${SCRIPT_PATH} @ONLY)

    add_custom_target(lint
        COMMAND ${SCRIPT_PATH} ${LINT_SOURCES}
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    )

    add_custom_target(lint_fix
        COMMAND ${SCRIPT_PATH} --fix ${LINT_SOURCES}
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    )
endfunction()

# The sources of the libraries, their tests and the bench programs, never the generated or fetched ones
get_filename_component(MICRAS_LIB_ROOT ${CMAKE_CURRENT_LIST_DIR}/.. ABSOLUTE)

file(GLOB_RECURSE MICRAS_LIB_FORMAT_FILES CONFIGURE_DEPENDS
    ${MICRAS_LIB_ROOT}/micras_*/*.cpp
    ${MICRAS_LIB_ROOT}/micras_*/*.hpp
    ${MICRAS_LIB_ROOT}/micras_*/*.tpp
    ${MICRAS_LIB_ROOT}/micras_*/*.h
    ${MICRAS_LIB_ROOT}/tests/*.cpp
    ${MICRAS_LIB_ROOT}/tests/*.hpp
    ${MICRAS_LIB_ROOT}/tests/*.h
)

file(GLOB MICRAS_LIB_BENCH_FILES CONFIGURE_DEPENDS
    ${MICRAS_LIB_ROOT}/reference/*/bench.cpp
    ${MICRAS_LIB_ROOT}/reference/common/*.hpp
)
list(APPEND MICRAS_LIB_FORMAT_FILES ${MICRAS_LIB_BENCH_FILES})

if(micras_lib_IS_TOP_LEVEL)
    file(GLOB_RECURSE MICRAS_LIB_LINT_SOURCES CONFIGURE_DEPENDS
        ${MICRAS_LIB_ROOT}/micras_*/*.cpp
        ${MICRAS_LIB_ROOT}/tests/*.cpp
    )

    micras_add_format_targets(${MICRAS_LIB_FORMAT_FILES})
    micras_add_lint_targets(SOURCES ${MICRAS_LIB_LINT_SOURCES})
endif()

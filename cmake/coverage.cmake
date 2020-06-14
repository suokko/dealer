# Helpers to build coverage binary for test coverage check.
# The design follows idea that coverage binary is separate executable and
# libraries with custom compiler flags. Then check_coverage depends on coverage
# binary instead of main binary.

include_guard(GLOBAL)
include(try_compile_cached)

set(COVERAGE_FAIL_MSG "***Error: There is no support for coverage build for your compiler. You can add support to cmake/coverage.cmake")

# Make interface libraries to propagate coverage compiler arguments to all
# targets
if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")

    set(COVERAGE_CPP_FLAGS -Og -g --coverage
        CACHE STRING "Coverage compiler flags appended to configuration flags")
    set(COVERAGE_LINK_FLAGS --coverage
        CACHE STRING "Coverage link flags appended to configuration flags")
    mark_as_advanced(COVERAGE_CPP_FLAGS)
    mark_as_advanced(COVERAGE_LINK_FLAGS)

    set(saved_LIB ${CMAKE_REQUIRED_LIBRARIES})
    list(APPEND CMAKE_REQUIRED_LIBRARIES ${COVERAGE_LINK_FLAGS})
    try_compiler_flag(CXX "${COVERAGE_CPP_FLAGS}" COVERAGE_CXX_SUPPORTED)
    set(CMAKE_REQUIRED_LIBRARIES ${saved_LIB})

    if (COVERAGE_CXX_SUPPORTED AND COVERAGE_C_SUPPORTED)
        set(COVERAGE_COMPILER_SUPPORTED ON)
    endif (COVERAGE_CXX_SUPPORTED AND COVERAGE_C_SUPPORTED)

    if (COVERAGE_COMPILER_SUPPORTED)
        add_library(coverage_options INTERFACE)
        target_compile_options(coverage_options INTERFACE ${COVERAGE_CPP_FLAGS})
        target_compile_definitions(coverage_options INTERFACE NDEBUG)

        if (CMAKE_VERSION VERSION_GREATER_EQUAL 3.13)
            target_link_options(coverage_options INTERFACE ${COVERAGE_LINK_FLAGS})
        else (CMAKE_VERSION VERSION_GREATER_EQUAL 3.13)
            target_link_libraries(coverage_options INTERFACE ${COVERAGE_LINK_FLAGS})
        endif (CMAKE_VERSION VERSION_GREATER_EQUAL 3.13)
    endif (COVERAGE_COMPILER_SUPPORTED)
endif (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")

# Add an error message if compiler isn't supported but user attempts to use
# coverage a target.
function(coverage_unsupported target)
    add_custom_Target(${target}
        COMMAND ${CMAKE_COMMAND} -E echo ${COVERAGE_FAIL_MSG}
        COMMAND false)
endfunction(coverage_unsupported)

set(COVERAGE_DIR ${CMAKE_CURRENT_LIST_DIR})

# Make sure there is no stale counts left from previously linked version which
# would generate stderr output and a test failure.
function(clear_target_counts target objtarget)
    get_target_property(LIBTYPE ${objtarget} TYPE)
    if (CMAKE_VERSION VERSION_LESS 3.15 AND NOT LIBTYPE STREQUAL "OBJECT_LIBRARY")
        message(FATAL_ERROR "Internal error in coverage.cmake: \
clear_target_counts(${target} ${objtarget}) must be called with OBJECT_LIBRARY in cmake \
version before 3.15. ${objtarget} type is ${LIBTYPE}")
    endif (CMAKE_VERSION VERSION_LESS 3.15 AND NOT LIBTYPE STREQUAL "OBJECT_LIBRARY")

    if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        add_custom_command(TARGET ${target}
            PRE_LINK
            COMMAND ${CMAKE_COMMAND} -D OBJS=$<TARGET_OBJECTS:${objtarget}>
                -D SUFFIX=gcda
                -P ${COVERAGE_DIR}/coverage_clear_counts.cmake
            VERBATIM)
    endif ()
endfunction(clear_target_counts)

# Helper to setup workaround object library to get object file paths to clear
# coverage count files from build directories.
function(exe_clear_counts target type)
    set(tgt "${target}.${type}")
    if (CMAKE_VERSION VERSION_LESS 3.15)
        set(DUMMY_ARG ${ARGN})
        list(FILTER DUMMY_ARG INCLUDE REGEX "^[A-Z_]*$")
        list(FILTER ARGN EXCLUDE REGEX "^[A-Z_]*$")
        # Work around for Xcode generator not accepting empty source list
        #list(APPEND DUMMY_ARG dummy.c)
        add_executable(${tgt} ${DUMMY_ARG})
        set(tgt "${tgt}.objlib")
        add_library(${tgt} EXCLUDE_FROM_ALL OBJECT ${ARGN})
        target_link_libraries(${target}.${type} ${tgt})
    else (CMAKE_VERSION VERSION_LESS 3.15)
        add_executable(${tgt} ${ARGN})
    endif (CMAKE_VERSION VERSION_LESS 3.15)
    target_link_libraries(${tgt} coverage_options)
    clear_target_counts(${target}.${type} ${tgt})
endfunction(exe_clear_counts)

function(lib_clear_counts target type)
    set(tgt "${target}.${type}")
    list(FIND ARGN "OBJECT" index)
    if (CMAKE_VERSION VERSION_LESS 3.15
            AND ${index} EQUAL -1)
        set(DUMMY_ARG ${ARGN})
        list(FILTER DUMMY_ARG INCLUDE REGEX "^[A-Z_]*$")
        list(FILTER ARGN EXCLUDE REGEX "^[A-Z_]*$")
        # Work around for Xcode generator not accepting empty source list
        #list(APPEND DUMMY_ARG dummy.c)
        add_library(${tgt} ${DUMMY_ARG})
        set(tgt "${tgt}.objlib")
        add_library(${tgt} OBJECT ${ARGN})
        target_link_libraries(${target}.${type} ${tgt})
    else (CMAKE_VERSION VERSION_LESS 3.15
            AND ${index} EQUAL -1)
        add_library(${tgt} ${ARGN})
    endif (CMAKE_VERSION VERSION_LESS 3.15
        AND ${index} EQUAL -1)
    target_link_libraries(${tgt} coverage_options)
    if (${index} EQUAL -1)
        clear_target_counts(${target}.${type} ${tgt})
    endif (${index} EQUAL -1)

endfunction(lib_clear_counts)


# Wrapper to add_executable which makes three target variants with different
# compiler options.
function(dealer_add_executable target)
    add_executable(${target} ${ARGN})

    # Do not built coverage targets by default
    list(INSERT ARGN 0 EXCLUDE_FROM_ALL)
    list(REMOVE_DUPLICATES ARGN)

    if (COVERAGE_COMPILER_SUPPORTED)
        exe_clear_counts(${target} cov ${ARGN})
    else (COVERAGE_COMPILER_SUPPORTED)
        coverage_unsupported(${target}.cov)
    endif (COVERAGE_COMPILER_SUPPORTED)

endfunction(dealer_add_executable)

# Wrapper to add_library which makes three target variants with different
# compiler options
function(dealer_add_library target)
    add_library(${target} ${ARGN})

    # Do not built coverage targets by default
    list(INSERT ARGN 0 EXCLUDE_FROM_ALL)
    list(REMOVE_DUPLICATES ARGN)

    if (COVERAGE_COMPILER_SUPPORTED)
        lib_clear_counts(${target} cov ${ARGN})
    else (COVERAGE_COMPILER_SUPPORTED)
        coverage_unsupported(${target}.cov)
    endif (COVERAGE_COMPILER_SUPPORTED)
endfunction(dealer_add_library)

function(get_target_name var target)
    set(${var} "${target}" PARENT_SCOPE)
    if (CMAKE_VERSION VERSION_LESS 3.15)
        get_target_property(LIBTYPE ${target} TYPE)
        if (NOT LIBTYPE STREQUAL "OBJECT_LIBRARY")
            set(${var} "${target}.objlib" PARENT_SCOPE)
        endif (NOT LIBTYPE STREQUAL "OBJECT_LIBRARY")
    endif (CMAKE_VERSION VERSION_LESS 3.15)
endfunction(get_target_name)

# Helper to link all variants with correct library variant
function(dealer_target_link_libraries target)
    target_link_libraries(${target} ${ARGN})
    foreach (arg IN LISTS ARGN)
        if (TARGET ${arg})
            get_target_property(LIBTYPE ${arg} TYPE)
            if (NOT LIBTYPE STREQUAL "INTERFACE_LIBRARY")
                list(APPEND cov_args "${arg}.cov")
                get_target_property(LIBTYPE ${arg}.cov TYPE)
                if (LIBTYPE STREQUAL "OBJECT_LIBRARY"
                        AND COVERAGE_COMPILER_SUPPORTED)
                    clear_target_counts(${target}.cov ${arg}.cov)
                endif (LIBTYPE STREQUAL "OBJECT_LIBRARY"
                    AND COVERAGE_COMPILER_SUPPORTED)
            else ()
                list(APPEND cov_args "${arg}")
            endif ()
        else ()
            list(APPEND cov_args "${arg}")
        endif ()
    endforeach (arg)
    if (COVERAGE_COMPILER_SUPPORTED)
        target_link_libraries(${target}.cov ${cov_args})
    endif (COVERAGE_COMPILER_SUPPORTED)
endfunction(dealer_target_link_libraries)

# helper to add compiler definitions to all variants
function(dealer_target_compile_definitions target)
    target_compile_definitions(${target} ${ARGN})
    if (COVERAGE_COMPILER_SUPPORTED)
        target_compile_options(${target}.cov ${ARGN})
        get_target_name(tgt ${target}.cov)
        target_compile_definitions(${tgt} ${ARGN})
    endif (COVERAGE_COMPILER_SUPPORTED)
endfunction(dealer_target_compile_definitions)

# helper to add compiler options to all variants
function(dealer_target_compile_options target)
    target_compile_options(${target} ${ARGN})
    if (COVERAGE_COMPILER_SUPPORTED)
        target_compile_options(${target}.cov ${ARGN})
        get_target_name(tgt ${target}.cov)
        target_compile_options(${tgt} ${ARGN})
    endif (COVERAGE_COMPILER_SUPPORTED)
endfunction(dealer_target_compile_options)

# helper to add include paths to all variants
function(dealer_target_include_directories target)
    target_include_directories(${target} ${ARGN})
    if (COVERAGE_COMPILER_SUPPORTED)
        target_include_directories(${target}.cov ${ARGN})
        get_target_name(tgt ${target}.cov)
        target_include_directories(${tgt} ${ARGN})
    endif (COVERAGE_COMPILER_SUPPORTED)
endfunction(dealer_target_include_directories)

# helper to modify target properties
function(dealer_set_target_properties target)
    set_target_properties(${target} ${ARGN})
    if (COVERAGE_COMPILER_SUPPORTED)
        set_target_properties(${target}.cov ${ARGN})
        get_target_name(tgt ${target}.cov)
        set_target_properties(${tgt} ${ARGN})
    endif (COVERAGE_COMPILER_SUPPORTED)
endfunction(dealer_set_target_properties)

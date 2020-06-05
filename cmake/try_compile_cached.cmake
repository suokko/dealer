# A simple helper to use try_compile with caching

include_guard(GLOBAL)

include(CMakeCheckCompilerFlagCommonPatterns)

# A wrapper around try_compile to cache positive test results
# First parameter must be language for the test like C or CXX
# Following parameters are same as cmake try_compile parameters with additional
# keys SOURCE_TEXT and FAIL_REGEX.
# SOURCE_TEXT must be followed by string which is the test source code.
# FAIL_REGEX is compile output which results to failure if detected in output
function(try_compile_cached _LANG _VAR)
    set(_try_compile_CACHE "")

    # Run test if variable isn't cached yet
    if (NOT DEFINED ${_VAR})

        # Read parameters and forward standard parameters
        set(_try_compile_key)
        set(_try_compile_key_std SOURCES)
        set(_try_compile_FAIL_REGEX)
        set(_try_compile_SOURCE_TEXT)
        set(_try_compile_ARG_VAR)
        foreach (arg ${ARGN})
            if ("${arg}" MATCHES "^(FAIL_REGEX|SOURCE_TEXT)$")
                set(_try_compile_key "${arg}")
                set(_try_compile_key_std)
            elseif("${arg}" MATCHES "^(CMAKE_FLAGS|COMPILE_DEFINATION|\
                LINK_LIBRARIES|COPY_FILE|COPY_FILE_ERROR|\
                ${_LANG}_STANDARD|${_LANG}_STANDARD_REQUIRED|\
                ${_LANG}_EXTENSIONS)$")
                list(APPEND _try_compile_ARG_VAR "${arg}")
                set(_try_compile_key_std "${arg}")
                set(_try_compile_key)
            elseif (_try_compile_key_std)
                list(APPEND _try_compile_ARG_VAR ${arg})
            elseif (_try_compile_key)
                list(APPEND _try_compile_${_try_compile_key} ${arg})
            endif ()
        endforeach ()

        set(_try_compile_DIR ${CMAKE_BINARY_DIR}${CMAKE_FILES_DIRECTORY})

        # Copy string sources to a file
        if (_try_compile_SOURCE_TEXT)
            list(GET CMAKE_${_LANG}_SOURCE_FILE_EXTENSIONS 0 _try_compile_EXT)
            set(_try_compile_SOURCES "${_try_compile_DIR}/test.${_try_compile_EXT}")
            file(WRITE ${_try_compile_SOURCES} "${_try_compile_SOURCE_TEXT}\n")

            list(INSERT _try_compile_ARG_VAR 0 ${_try_compile_SOURCES})
        endif (_try_compile_SOURCE_TEXT)

        list(APPEND _try_compile_ARG_VAR OUTPUT_VARIABLE _try_compile_OUTPUT)

        if (CMAKE_VERSION VERSION_GREATER_EQUAL 3.15)
            message(DEBUG "try_compile(${_VAR} ${CMAKE_BINARY_DIR} "
                "${_try_compile_ARG_VAR})")
        endif (CMAKE_VERSION VERSION_GREATER_EQUAL 3.15)

        try_compile(${_VAR}
            ${_try_compile_DIR}
            ${_try_compile_ARG_VAR})

        foreach (_fail ${_try_compile_FAIL_REGEX})
            if (_try_compile_OUTPUT MATCHES "${_fail}")
                set(${_VAR} OFF)
            endif (_try_compile_OUTPUT MATCHES "${_fail}")
        endforeach ()

        # Report compile results
        if (${_VAR})
            file(APPEND ${_try_compile_DIR}/CMakeOutput.log
                "Performing ${_LANG} Compile Test ${VAR} succeeded with the following output:\n"
                "${_try_compile_OUTPUT}\n")
        else (${_VAR})
            file(APPEND ${_try_compile_DIR}/CMakeError.log
                "Performing ${_LANG} Compile Test ${VAR} failed with the following output:\n"
                "${_try_compile_OUTPUT}\n")
        endif (${_VAR})

        # Now set value to cache
        set(${VAR} ${${_VAR}} CACHE INTERNAL "${_LANG} compile test ${VAR}")

    else ()
        set(_try_compile_CACHE " (Cached)")
    endif ()

    # Report result of check in the terminal log
    if (NOT CMAKE_REQUIRED_QUIET)
        string(LENGTH ${_VAR} _try_compile_LEN)

        # Cache the longest text length
        if (NOT DEFINED TRY_COMPILE_LONGEST_VAR
                OR _try_compile_LEN GREATER TRY_COMPILE_LONGEST_VAR)
            set(TRY_COMPILE_LONGEST_VAR ${_try_compile_LEN}
                CACHE INTERNAL "The longest cache variable to align message")
        endif ()

        # Align variable lengths to make it easier to check success/failure
        # status
        set(_try_compile_ALIGN "")
        while (_try_compile_LEN LESS ${TRY_COMPILE_LONGEST_VAR})
            math(EXPR _try_compile_LEN "${_try_compile_LEN} + 1")
            string(APPEND _try_compile_ALIGN " ")
        endwhile ()

        #Align language names too
        set(_try_compile_ALIGN_LANG "")
        string(LENGTH ${_LANG} _try_compile_LLEN)
        while (_try_compile_LLEN LESS 7) # Fortran
            math(EXPR _try_compile_LLEN "${_try_compile_LLEN} + 1")
            string(APPEND _try_compile_ALIGN_LANG " ")
        endwhile ()

        if (${_VAR})
            set(_try_compile_RESULT "Successed")
        else (${_VAR})
            set(_try_compile_RESULT "Failed")
        endif (${_VAR})
        message(STATUS "Performing ${_LANG} Test${_try_compile_ALIGN_LANG} "
            "${_VAR}${_try_compile_ALIGN} - ${_try_compile_RESULT}${_try_compile_CACHE}")
    endif (NOT CMAKE_REQUIRED_QUIET)

endfunction(try_compile_cached)

# Simple helper to replace check_${_LANG}_compiler_flags
#
function(try_compiler_flag _LANG _FLAG _RESULT)
    # Make sure compiler uses correct locale
    set(_try_flags_LOCALE LC_ALL LC_MESSAGES LANG)
    foreach (flag ${_try_flags_LOCALE})
        set(_try_flags_SAVE_${flag} "$ENV{${flag}}")
        set("$ENV{${flag}}" C)
    endforeach()
    check_compiler_flag_common_patterns(_try_flags_common_patterns)

    set(_try_flags_LIBS)
    if (CMAKE_REQUIRED_LIBRARIES)
        set(_try_flags_LIBS LINK_LIBRARIES ${CMAKE_REQUIRED_LIBRARIES})
    endif (CMAKE_REQUIRED_LIBRARIES)

    try_compile_cached(${_LANG} ${_RESULT}
        CMAKE_FLAGS ${_FLAG}
        ${_try_flags_LIBS}
        SOURCE_TEXT "int main(void) {return 0;}"
        ${_try_flags_common_patterns})

    foreach (flag ${_try_flags_LOCALE})
        set(ENV{${flag}} _try_flags_SAVE_${flag})
    endforeach()
endfunction(try_compiler_flag)

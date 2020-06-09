# A simple helper to use try_compile with caching

include_guard(GLOBAL)

include(CMakeCheckCompilerFlagCommonPatterns)

macro(try_compile_parse_args WRAPPER FORWARD)

    # Read parameters and forward standard parameters
    set(key)
    set(key_std SOURCES)
    foreach (VAR ${WRAPPER})
        set(${VAR})
    endforeach ()
    set(_WRAPPER ${WRAPPER})
    list(JOIN _WRAPPER "|" WREGEXP)
    set(WREGEXP "^(${WREGEXP})$")
    set(_FORWARD ${FORWARD})
    list(JOIN _FORWARD "|" FREGEXP)
    set(FREGEXP "^(${FREGEXP})$")
    set(ARG_VAR)
    foreach (arg ${ARGN})
        if ("${arg}" MATCHES ${WREGEXP})
            set(key "${arg}")
            set(key_std)
        elseif("${arg}" MATCHES ${FREGEXP})
            list(APPEND ARG_VAR "${arg}")
            set(key_std "${arg}")
            set(key)
        elseif (key_std)
            list(APPEND ARG_VAR ${arg})
        elseif (key)
            list(APPEND ${key} ${arg})
        endif ()
    endforeach ()

    set(DIR ${CMAKE_BINARY_DIR}${CMAKE_FILES_DIRECTORY})
    get_property(IDIRS DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        PROPERTY INCLUDE_DIRECTORIES)
    get_property(LDIRS DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        PROPERTY LINK_DIRECTORIES)
    list(APPEND IDIRS ${CMAKE_REQUIRED_INCLUDES})
    if (LDIRS)
        list(APPEND ARG_VAR LINK_LIBRARIES "-L${LDIRS}")
    endif ()
    if (IDIRS)
        set(IDIRS "-DINCLUDE_DIRECTORIES:STRING=${IDIRS}")
        list(APPEND ARG_VAR CMAKE_FLAGS "${IDIRS}")
    endif ()

    # Copy string sources to a file
    if (SOURCE_TEXT)
        list(GET CMAKE_${_LANG}_SOURCE_FILE_EXTENSIONS 0 EXT)
        set(SOURCES "${DIR}/test.${EXT}")
        file(WRITE ${SOURCES} "${SOURCE_TEXT}\n")

        list(INSERT ARG_VAR 0 ${SOURCES})
    endif (SOURCE_TEXT)
endmacro(try_compile_parse_args)

function(try_compile_report _VAR)
    list(LENGTH ARGN ARG_LEN)
    if (ARG_LEN GREATER 1)
        list(GET ARGN 0 _RUN_VAR)
        list(GET ARGN 1 _COMPILE_VAR)
    endif ()

    foreach (_fail ${FAIL_REGEX})
        if (OUTPUT MATCHES "${_fail}")
            set(${_VAR} OFF)
        endif (OUTPUT MATCHES "${_fail}")
    endforeach ()

    # Report compile results
    if (SOURCE_TEXT)
        string(PREPEND SOURCE_TEXT "Source code:\n")
        string(APPEND SOURCE_TEXT "\n")
    endif (SOURCE_TEXT)
    # Find out compilation result
    if (DEFINED _COMPILE_VAR)
        set(SUCCESS ${${_COMPILE_VAR}})
    else ()
        set(SUCCESS ${${_VAR}})
    endif ()
    # Generate text feedback for compilation result
    set(COMPILE_REPORT "succeeded")
    if (NOT SUCCESS)
        set(COMPILE_REPORT "failed")
    endif ()
    # Find out test run result if applicable
    set(RUN_REPORT)
    if (DEFINED _RUN_VAR)
        if (NOT ${_RUN_VAR} EQUAL 0)
            set(SUCCESS FALSE)
        endif ()
        set(RUN_REPORT "Runtime output:\n${RUN_OUTPUT}\n\
Return code: ${${_RUN_VAR}}\n")
    endif ()
    # Select output file
    if (SUCCESS)
        set(OUT ${DIR}/CMakeOutput.log)
    else ()
        set(OUT ${DIR}/CMakeError.log)
    endif ()
    # Write the report
    file(APPEND ${OUT}
        "Performing ${_LANG} Compile Test ${_VAR} ${COMPILE_REPORT} with the following output:\n"
        "${OUTPUT}\n${SOURCE_TEXT}\n${RUN_REPORT}")

    # Forward try_run variables to a single variable
    if (DEFINED _COMPILE_VAR)
        set(${_VAR} ${SUCCESS} CACHE INTERNAL "Result of try_run_cached")
    endif ()

endfunction(try_compile_report)

function(try_compile_report_status _VAR CACHESTR)
    if (NOT CMAKE_REQUIRED_QUIET)
        string(LENGTH ${_VAR} LEN)

        # Cache the longest text length
        if (NOT DEFINED TRY_COMPILE_LONGEST_VAR
                OR LEN GREATER TRY_COMPILE_LONGEST_VAR)
            set(TRY_COMPILE_LONGEST_VAR ${LEN}
                CACHE INTERNAL "The longest cache variable to align message")
        endif ()

        # Align variable lengths to make it easier to check success/failure
        # status
        set(ALIGN "")
        while (LEN LESS ${TRY_COMPILE_LONGEST_VAR})
            math(EXPR LEN "${LEN} + 1")
            string(APPEND ALIGN " ")
        endwhile ()

        #Align language names too
        set(ALIGN_LANG "")
        string(LENGTH ${_LANG} LLEN)
        while (LLEN LESS 7) # Fortran
            math(EXPR LLEN "${LLEN} + 1")
            string(APPEND ALIGN_LANG " ")
        endwhile ()

        if (${_VAR})
            set(RESULT "Successed")
        else (${_VAR})
            set(RESULT "Failed")
        endif (${_VAR})
        message(STATUS "Performing ${_LANG} Test${ALIGN_LANG} "
            "${_VAR}${ALIGN} - ${RESULT}${CACHESTR}")
    endif (NOT CMAKE_REQUIRED_QUIET)
endfunction(try_compile_report_status)


# A wrapper around try_compile to cache positive test results
# First parameter must be language for the test like C or CXX
# Following parameters are same as cmake try_compile parameters with additional
# keys SOURCE_TEXT and FAIL_REGEX.
# SOURCE_TEXT must be followed by string which is the test source code.
# FAIL_REGEX is compile output which results to failure if detected in output
function(try_compile_cached _LANG _VAR)
    set(CACHESTR "")

    # Run test if variable isn't cached yet
    if (NOT DEFINED ${_VAR})

        try_compile_parse_args("FAIL_REGEX;SOURCE_TEXT"
            "CMAKE_FLAGS;COMPILE_DEFINITIONS;LINK_LIBRARIES;COPY_FILE;COPY_FILE_ERROR;\
${_LANG}_STANDARD;${_LANG}_STANDARD_REQUIRED;${_LANG}_EXTENSIONS" ${ARGN})

        list(APPEND ARG_VAR OUTPUT_VARIABLE OUTPUT)

        if (CMAKE_VERSION VERSION_GREATER_EQUAL 3.15)
            message(DEBUG "try_compile(${_VAR} ${CMAKE_BINARY_DIR} ${ARG_VAR})")
        endif ()

        try_compile(${_VAR}
            ${CMAKE_BINARY_DIR}
            ${ARG_VAR})

        try_compile_report(${_VAR})

    else ()
        set(CACHESTR " (Cached)")
    endif ()

    # Report result of check in the terminal log
    try_compile_report_status(${_VAR} "${CACHESTR}")

endfunction(try_compile_cached)

function(try_run_cached _LANG _VAR)
    set(CACHESTR "")

    # Run test if variable isn't cached yet
    if (NOT DEFINED ${_VAR})

        try_compile_parse_args("FAIL_REGEX;SOURCE_TEXT"
            "CMAKE_FLAGS;COMPILE_DEFINITIONS;LINK_LIBRARIES;ARGS" ${ARGN})

        list(APPEND ARG_VAR COMPILE_OUTPUT_VARIABLE OUTPUT)
        list(APPEND ARG_VAR RUN_OUTPUT_VARIABLE RUN_OUTPUT)

        if (CMAKE_VERSION VERSION_GREATER_EQUAL 3.15)
            message(DEBUG "try_run(${_VAR}_RUN ${_VAR}_COMPILE ${CMAKE_BINARY_DIR} ${ARG_VAR})")
        endif (CMAKE_VERSION VERSION_GREATER_EQUAL 3.15)

        try_run(${_VAR}_RUN ${_VAR}_COMPILE
            ${CMAKE_BINARY_DIR}
            ${ARG_VAR})

        try_compile_report(${_VAR} ${_VAR}_RUN ${_VAR}_COMPILE)

    else ()
        set(CACHESTR " (Cached)")
    endif ()

    # Report result of check in the terminal log
    try_compile_report_status(${_VAR} "${CACHESTR}")

endfunction(try_run_cached)

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
        COMPILE_DEFINITIONS ${_FLAG}
        ${_try_flags_LIBS}
        SOURCE_TEXT "int main(void) {return 0;}"
        ${_try_flags_common_patterns})

    foreach (flag ${_try_flags_LOCALE})
        set(ENV{${flag}} ${_try_flags_SAVE_${flag}})
    endforeach()
endfunction(try_compiler_flag)

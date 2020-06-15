
set(VERSION_SELF_FILE ${CMAKE_CURRENT_LIST_FILE})

function(configure_version _FILE)
    string(REGEX REPLACE "\.in$" "" _FILE_CONF ${_FILE})
    string(REGEX REPLACE "\.in$" "" _FILE_SRC ${_FILE_CONF})
    configure_file("${CMAKE_CURRENT_SOURCE_DIR}/${_FILE}"
        "${CMAKE_CURRENT_BINARY_DIR}/${_FILE_CONF}"
        @ONLY)

    add_custom_command(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/${_FILE_SRC}" always_rebuild
        COMMAND "${CMAKE_COMMAND}" -DGIT_EXECUTABLE=${GIT_EXECUTABLE}
            -DSRC=${CMAKE_CURRENT_BINARY_DIR}/${_FILE_CONF}
            -DDST=${CMAKE_CURRENT_BINARY_DIR}/${_FILE_SRC}
            -DPROJECT_VERSION=${PROJECT_VERSION}
            -P "${VERSION_SELF_FILE}"
        DEPENDS "${VERSION_SELF_FILE}"
            "${CMAKE_CURRENT_BINARY_DIR}/${_FILE_CONF}"
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        VERBATIM)

endfunction(configure_version)


if (DST AND SRC)
    execute_process(COMMAND "${GIT_EXECUTABLE}" describe --dirty
        RESULT_VARIABLE RES
        OUTPUT_VARIABLE OUT)
    if (NOT RES EQUAL "0")
        message(FATAL_ERROR "git (${GIT_EXECUTABLE} describe --dirty) failed.\n${OUT}")
    endif ()
    string(STRIP ${OUT} GIT_VERSION)
    configure_file(${SRC} ${DST})
endif ()

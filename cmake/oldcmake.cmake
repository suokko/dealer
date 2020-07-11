# Some helpers to support old cmake versions

macro(list_join _LIST _SEP _OUT)
    if (CMAKE_VERSION VERSION_GREATER 3.12)
        list(JOIN ${_LIST} ${_SEP} ${_OUT})
    else ()
        set(_list_join_FIRST TRUE)
        foreach (_list_join_ELEM ${${_LIST}})
            if (_list_join_FIRST)
                set(${_OUT} "${_list_join_ELEM}")
                set(_list_join_FIRST FALSE)
            else ()
                set(${_OUT} "${${_OUT}}${_SEP}${_list_join_ELEM}")
            endif ()
        endforeach ()
        unset(_list_join_ELEM)
        unset(_list_join_FIRST)
    endif ()
endmacro(list_join)

macro(string_append _STR _INPUT)
    if (CMAKE_VERSION VERSION_GREATER 3.4)
        string(APPEND ${_STR} ${_INPUT})
    else ()
        set(${_STR} "${${_STR}}${_INPUT}")
    endif ()
endmacro(string_append)

macro(string_prepend _STR _INPUT)
    if (CMAKE_VERSION VERSION_GREATER 3.10)
        string(PREPEND ${_STR} ${_INPUT})
    else ()
        set(${_STR} "${_INPUT}${${_STR}}")
    endif ()
endmacro(string_prepend)

# Helper to switch OBJECT_LIBRARIES to static on old cmake
function(add_library_old _TGT)
    if (CMAKE_VERSION VERSION_GREATER 3.12)
        add_library(${_TGT} ${ARGN})
    else ()
        set(_ARGN ${ARGN})
        list(FIND _ARGN "OBJECT" index)
        if (NOT index EQUAL -1)
            list(REMOVE_AT _ARGN ${index})
            list(INSERT _ARGN ${index} "STATIC")
        endif ()
        add_library(${_TGT} ${_ARGN})
        set_target_properties(${_TGT} PROPERTIES WHOLE_ARCHIVE TRUE)
    endif ()
endfunction(add_library_old)

function(target_link_libraries_old _TGT)
    if (CMAKE_VERSION VERSION_GREATER 3.12)
        target_link_libraries(${_TGT} ${ARGN})
    else ()
        set(_ARGN)
        foreach (_A ${ARGN})
            if (TARGET ${_A})
                get_target_property(LIBTYPE ${_A} TYPE)
                set(WA FALSE)
                if (LIBTYPE STREQUAL "STATIC_LIBRARY")
                    get_target_property(WA ${_A} WHOLE_ARCHIVE)
                endif ()
                if (WA)
                    list(APPEND _ARGN "-Wl,--whole-archive")
                endif ()
                list(APPEND _ARGN ${_A})
                if (WA)
                    list(APPEND _ARGN "-Wl,--no-whole-archive")
                endif ()
            else ()
                list(APPEND _ARGN ${_A})
            endif ()
        endforeach ()
        target_link_libraries(${_TGT} ${_ARGN})
    endif ()
endfunction(target_link_libraries_old)

function(target_link_directories_old TARGET TYPE)
    if (CMAKE_VERSION VERSION_GREATER 3.13)
        target_link_directories(${TARGET} ${TYPE} ${ARGN})
    else ()
        link_directories(${ARGN})
    endif ()
endfunction (target_link_directories_old)

function(target_link_options_old)
    if (CMAKE_VERSION VERSION_GREATER 3.13)
        target_link_options(${ARGN})
    else (CMAKE_VERSION VERSION_GREATER 3.13)
        target_link_libraries(${ARGN})
    endif (CMAKE_VERSION VERSION_GREATER 3.13)
endfunction(target_link_options_old)

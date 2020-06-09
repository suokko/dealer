
# Check for architecture optimization flag support
function(flags_check_config _NAME CPP_FLAGS FLAG_HELP)
    list(LENGTH ARGN _LEN)
    if (_LEN GREATER 0)
        list(GET ARGN 0 OPTION_HELP)
    endif (_LEN GREATER 0)
    if (_LEN GREATER 1)
    list(GET ARGN 1 OPTION_DEFAULT)
    endif (_LEN GREATER 1)

    if (NOT FLAGS_${_NAME} STREQUAL CPP_FLAGS)
        # Clear variables if defaults changed
        unset(OPTIMIZE_${_NAME}_C CACHE)
        unset(OPTIMIZE_${_NAME}_CXX CACHE)
        unset(OPTIMIZE_${_NAME}_CPP_FLAGS CACHE)
        set(FLAGS_${_NAME} ${CPP_FLAGS} CACHE INTERNAL "" FORCE)
    endif (NOT FLAGS_${_NAME} STREQUAL CPP_FLAGS)

    set(OPTIMIZE_${_NAME}_CPP_FLAGS ${CPP_FLAGS}
        CACHE STRING ${FLAG_HELP})
    mark_as_advanced(OPTIMIZE_${_NAME}_CPP_FLAGS)


    try_compiler_flag(C "${OPTIMIZE_${_NAME}_CPP_FLAGS}" OPTIMIZE_${_NAME}_C)
    try_compiler_flag(CXX "${OPTIMIZE_${_NAME}_CPP_FLAGS}" OPTIMIZE_${_NAME}_CXX)
    if (OPTIMIZE_${_NAME}_C AND OPTIMIZE_${_NAME}_CXX)
        if (OPTION_HELP)
            option(OPTIMIZE_${_NAME} ${OPTION_HELP} ${OPTION_DEFAULT})
        else (OPTION_HELP)
            set(OPTIMIZE_${_NAME} ON PARENT_SCOPE)
        endif (OPTION_HELP)
    else (OPTIMIZE_${_NAME}_C AND OPTIMIZE_${_NAME}_CXX)
        set(OPTIMIZE_${_NAME} OFF PARENT_SCOPE)
    endif (OPTIMIZE_${_NAME}_C AND OPTIMIZE_${_NAME}_CXX)
endfunction(flags_check_config)


macro(flags_validate_default VAR)
    set(_flags_temp)
    foreach (FLAG ${${VAR}})
        string(REGEX REPLACE "[-=]" "_" _NAME ${FLAG})
        try_compiler_flag(C ${FLAG} CHECK${_NAME}_C)
        try_compiler_flag(CXX ${FLAG} CHECK${_NAME}_CXX)
        if (CHECK${_NAME}_C AND CHECK${_NAME}_CXX)
            list(APPEND _flags_temp ${FLAG})
        endif (CHECK${_NAME}_C AND CHECK${_NAME}_CXX)
    endforeach ()
    set(${VAR} ${_flags_temp})
    unset(_flags_temp)
endmacro(flags_validate_default)


macro(flags_add_optimization interface_target)
    # User configuration for optimization flags
    flags_check_config(NATIVE_ONLY
        "${COMPILER_CONFIG_native_only}"
        "Compiler flags used to compile native only binary"
        "Make dealer to include only native optimiation to the processor"
        OFF)

    foreach (CFG IN LISTS COMPILER_CONFIGS)
        if (NOT CFG STREQUAL "default")
            flags_check_config(${CFG}
                "${COMPILER_CONFIG_${CFG}}"
                "Compiler flags used to compile ${CFG} optimization version"
                "Enable optimization for ${CFG} instruction set"
                ON)
        else (NOT CFG STREQUAL "default")
            flags_check_config(${CFG}
                "${COMPILER_CONFIG_${CFG}}"
                "Compiler flags used to compile ${CFG} optimization version")
        endif (NOT CFG STREQUAL "default")
    endforeach (CFG IN LISTS COMPILER_CONFIGS)

    # Make sure we only use selected configs
    if (OPTIMIZE_NATIVE_ONLY)
        set(COMPILER_CONFIGS default)
        list(APPEND OPTIMIZE_default_CPP_FLAGS ${COMPILER_CONFIG_native_only})
        list(REMOVE_DUPLICATES OPTIMIZE_default_CPP_FLAGS)
    else (OPTIMIZE_NATIVE_ONLY)

        foreach (CFG IN LISTS COMPILER_CONFIGS)
            if (OPTIMIZE_${CFG})
                list(APPEND temp_CFGS ${CFG})
            endif (OPTIMIZE_${CFG})
        endforeach (CFG IN LISTS COMPILER_CONFIGS)

        set(COMPILER_CONFIGS ${temp_CFGS})
        unset(temp_CFGS)
    endif (OPTIMIZE_NATIVE_ONLY)

    add_library(${interface_target} INTERFACE)
    target_compile_options(${interface_target} INTERFACE ${OPTIMIZE_default_CPP_FLAGS})
endmacro(flags_add_optimization)

if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    set(COMPILER_CONFIGS default sse2 popcnt sse4 bmi avx2)

    set(COMPILER_CONFIG_default -mfpmath=sse -ffast-math)
    flags_validate_default(COMPILER_CONFIG_default)

    set(COMPILER_CONFIG_native_only -march=native -mtune=native)
    set(COMPILER_CONFIG_sse2 ${COMPILER_CONFIG_default} -msse -msse2)
    set(COMPILER_CONFIG_popcnt ${COMPILER_CONFIG_sse2} -msse3 -mssse3 -mpopcnt)
    set(COMPILER_CONFIG_sse4 ${COMPILER_CONFIG_popcnt} -msse4.1 -msse4.2)
    set(COMPILER_CONFIG_bmi ${COMPILER_CONFIG_sse4} -mbmi -mlzcnt)
    set(COMPILER_CONFIG_avx2 ${COMPILER_CONFIG_bmi} -mmovbe -mbmi2 -mavx -mavx2 -mfma)
else ()
    message(STATUS "No optimization flag support for ${CMAKE_CXX_COMPILER_ID}.
See: ${CMAKE_CURRENT_LIST_FILE}")
    set(COMPILER_CONFIGS default)
    set(COMPILER_CONFIG_default)
endif ()


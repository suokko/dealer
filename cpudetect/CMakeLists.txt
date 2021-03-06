
# Helper library for shared cpu object code
dealer_add_library(cpudetect OBJECT EXCLUDE_FROM_ALL
    detect.cpp
    detect.h
    entry.h
    )

if (LTO_SUPPORTED)
    dealer_set_target_properties(cpudetect PROPERTIES INTERPROCEDURAL_OPTIMIZATION TRUE)
endif ()

dealer_target_link_libraries(cpudetect default_flags)
dealer_target_include_directories(cpudetect PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

set(CPU_SRC
    main.cpp
    )

add_executable(testcpu ${CPU_SRC})
target_link_libraries_old(testcpu cpudetect default_flags)
if (LTO_SUPPORTED)
    set_target_properties(testcpu PROPERTIES INTERPROCEDURAL_OPTIMIZATION TRUE)
endif ()

set(CPUDETECT_CONFIGS ${COMPILER_CONFIGS})
list(APPEND CPUDETECT_CONFIGS slowmul popcnt)
set(COMPILER_CONFIG_slowmul ${COMPILER_CONFIG_default})
list(REMOVE_DUPLICATES CPUDETECT_CONFIGS)
foreach (CFG ${CPUDETECT_CONFIGS})
    add_library_old(target_${CFG} OBJECT EXCLUDE_FROM_ALL
        target.cpp
        target.h
        ../bittwiddle.cpp
        )
    target_compile_definitions(target_${CFG} PRIVATE MVDEFAULT=${CFG} MV${CFG}=1)
    target_compile_options(target_${CFG} PRIVATE ${COMPILER_CONFIG_${CFG}})
    target_include_directories(target_${CFG} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    if (LTO_SUPPORTED)
        set_target_properties(target_${CFG} PROPERTIES INTERPROCEDURAL_OPTIMIZATION TRUE)
    endif ()
    set_target_properties(target_${CFG} PROPERTIES CXX_EXTENSIONS ON)
    target_link_libraries_old(testcpu target_${CFG})
endforeach (CFG ${CPUDETECT_CONFIGS})

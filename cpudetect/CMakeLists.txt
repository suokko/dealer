
# Helper library for shared cpu object code
dealer_add_library(cpudetect OBJECT EXCLUDE_FROM_ALL
    detect.cpp
    detect.h
    entry.h
    )
dealer_target_link_libraries(cpudetect default_flags)

set(CPU_SRC
    main.cpp
    )

add_executable(testcpu ${CPU_SRC})
target_link_libraries(testcpu cpudetect default_flags)

set(CPUDETECT_CONFIGS ${COMPILER_CONFIGS})
list(APPEND CPUDETECT_CONFIGS slowmul)
set(COMPILER_CONFIG_slowmul ${COMPILER_CONFIG_default})
foreach (CFG ${CPUDETECT_CONFIGS})
    add_library(target_${CFG} OBJECT EXCLUDE_FROM_ALL target.cpp target.h)
    target_compile_definitions(target_${CFG} PRIVATE MVDEFAULT=${CFG})
    target_compile_options(target_${CFG} PRIVATE ${COMPILER_CONFIG_${CFG}})
    target_link_libraries(testcpu target_${CFG})
endforeach (CFG ${CPUDETECT_CONFIGS})

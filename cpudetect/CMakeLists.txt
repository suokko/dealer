
# Helper library for shared cpu object code
dealer_add_library(cpudetect OBJECT EXCLUDE_FROM_ALL
    detect.cpp
    detect.h
    entry.h
    )

set(CPU_SRC
    main.cpp
    )

add_executable(testcpu ${CPU_SRC})
target_link_libraries(testcpu cpudetect)

set(CPUDETECT_CONFIGS default slowmul sse2 sse3 popcnt sse4 avx avx2)

foreach (CFG ${CPUDETECT_CONFIGS})
    add_library(target_${CFG} OBJECT EXCLUDE_FROM_ALL target.cpp)
    target_compile_definitions(target_${CFG} PRIVATE MVDEFAULT=${CFG})
    target_link_libraries(testcpu target_${CFG})
endforeach (CFG ${CPUDETECT_CONFIGS})

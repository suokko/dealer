
# Helper to locate libdds files

find_path(PCGCXX_INCLUDE_DIR pcg_random.hpp
    HINTS $ENV{PCGCXXDIR}
    PATH_SUFFIXES include
    DOC "The directory of libpcg-cpp development headers"
    )

set(USE_SYSTEM_PCG_INIT OFF)
if (PCGCXX_INCLUDE_DIR)
    set(USE_SYSTEM_PCG_INIT ON)
endif ()

if (PCGCXX_REQUIRED)
    set(USE_SYSTEM_PCGCXX TRUE)
else ()
    option(USE_SYSTEM_PCGCXX "Use system version of libpcg-cpp instead of git submodule"
        ${USE_SYSTEM_PCG_INIT})
endif ()

add_library(PCGCXX INTERFACE)
if (USE_SYSTEM_PCGCXX)
    target_include_directories(PCGCXX INTERFACE "${PCGCXX_INCLUDE_DIR}")
else ()
    target_include_directories(PCGCXX INTERFACE "pcg-cpp/include")
endif ()

set(PCGCXX_REQUIRED ${USE_SYSTEM_PCGCXX})

include(FindPackageHandleStandardArgs)

find_package_handle_standard_args(PCGCXX REQUIRED_VARS PCGCXX_INCLUDE_DIR)

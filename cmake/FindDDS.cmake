
# Helper to locate libdds files

find_path(DDS_INCLUDE_DIR dll.h
    HINTS $ENV{DDSDIR}
    PATH_SUFFIXES include/dds include
    DOC "The path of libdds.{so,dll}"
    )

find_library(DDS_LIBRARY
    NAMES dds
    HINTS $ENV{DDSDIR}
    DOC "The directory of libdds development headers"
    )

set(USE_SYSTEM_DDS_INIT OFF)
if (DDS_LIBRARY AND DDS_INCLUDE_DIR)
    set(USE_SYSTEM_DDS_INIT ON)
endif ()

if (DDS_REQUIRED)
    set(USE_SYSTEM_DDS TRUE)
else ()
    option(USE_SYSTEM_DDS "Use system version of libdds instead of git submodule"
        ${USE_SYSTEM_DDS_INIT})
endif ()

if (USE_SYSTEM_DDS)
    add_library(DDS INTERFACE)
    target_include_directories(DDS INTERFACE "${DDS_INCLUDE_DIR}")
    #Not used: target_link_libraries(DDS::DDS INTERFACE "${DDS_LIBRARY}")
endif ()

set(DDS_REQUIRED ${USE_SYSTEM_DDS})

include(FindPackageHandleStandardArgs)

find_package_handle_standard_args(DDS REQUIRED_VARS DDS_LIBRARY DDS_INCLUDE_DIR)

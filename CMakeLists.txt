cmake_minimum_required(VERSION 3.12)

# Make sure cmake finds custom modules
list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

# Make sure compiler selects c++11 standard
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Use new policy for version number variables
cmake_policy(SET CMP0048 NEW)

# Declare project information.
project(dealer
    VERSION 0.99.0
    DESCRIPTION "Generates bridge hands for statistical analyze"
    HOMEPAGE_URL "https://github.com/suokko/dealer"
    LANGUAGES CXX C)

# Set symbols hidden by default to eliminate unnecessary exported symbols
cmake_policy(SET CMP0063 NEW)
set(CMAKE_CXX_VISIBILITY_PRESET internal)
set(CMAKE_VISIBILITY_INLINES_HIDDEN 1)

# Find out version number where source code is coming from
find_package(Git QUIET)
set(SRC_VERSION "unknown")
set(RES "-1")
if (GIT_FOUND)

    execute_process(COMMAND "${GIT_EXECUTABLE}" describe --dirty
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        RESULT_VARIABLE RES
        OUTPUT_VARIABLE OUT)

    if (RES EQUAL "0")
        string(STRIP ${OUT} SRC_VERSION)
        configure_file(${CMAKE_CURRENT_SOURCE_DIR}/SourceVersion.cmake.in
            ${CMAKE_CURRENT_SOURCE_DIR}/SourceVersion.cmake @ONLY)
    endif (RES EQUAL "0")
endif (GIT_FOUND)
if (NOT RES EQUAL "0")
    include(SourceVersion.cmake OPTIONAL)
endif (NOT RES EQUAL "0")

# Make sure submodules are checked out
set(SUBMODULES OFF)
if (GIT_FOUND AND EXISTS "${PROJECT_SOURCE_DIR}/.git")
    set(SUBMODULES ON)

    add_custom_command(
        OUTPUT submodule
        COMMAND ${GIT_EXECUTABLE} submodule update --init --recursive
        COMMAND ${CMAKE_COMMAND} -E touch ${CMAKE_CURRENT_BINARY_DIR}/submodule
        WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
        DEPENDS .gitmodules)

endif (GIT_FOUND AND EXISTS "${PROJECT_SOURCE_DIR}/.git")

message(STATUS "Configuring ${PROJECT_NAME} ${PROJECT_VERSION} - ${PROJECT_DESCRIPTION}")
message(STATUS "Source code is from version ${SRC_VERSION}")

# Find required dependencies
find_package(BISON REQUIRED)
find_package(FLEX REQUIRED)
find_package(Perl QUIET)

#import cmake helpers
include(GNUInstallDirs)
cmake_policy(SET CMP0069 NEW)
include(CheckIPOSupported)
include(try_compile_cached)
include(optimization_flags)
include(coverage)
include(pod2man)

# Check that there is a working std::thread. The mingw might be missing std::thread support if using wrong threading model.
try_compile_cached(CXX STD_THREAD_SUPPORTED
    CXX_STANDARD 11
    CXX_STANDARD_REQUIRED ON
    SOURCE_TEXT
"#include <thread>
int main(void) {
    int cores = std::thread::hardware_concurrency();
    return 0;
}")

if (NOT STD_THREAD_SUPPORTED)
    message(FATAL_ERROR "Compiler is missing std::thread support. If you use mingw then selecting alternative threading model potentially fixes the issue")
endif (NOT STD_THREAD_SUPPORTED)

# Check if getopt is supported
try_compile_cached(C GETOPT_SUPPORTED
    SOURCE_TEXT
"#include <unistd.h>
int main(int argc, char* const argv[]) {
    const char *opt = \"h\";
    return getopt(argc, argv, opt);
}")

add_library(wmain_flags INTERFACE)
if (WIN32)
    # Check if calling from wmain to main works
    try_run_cached(C CHECK_WMAIN_SUPPORTED
        SOURCE_TEXT "int rv = 1;
int main(void) {return rv;}
int wmain(void) {rv = 0; return main();}"
        CMAKE_FLAGS -municode
        LINK_LIBRARIES -municode)

    if (NOT CHECK_WMAIN_SUPPORTED)
        message(FATAL_ERROR "Failed to use wmain entry for windows.")
    endif ()

    target_compile_options(wmain_flags INTERFACE -municode)
    if (CMAKE_VERSION VERSION_GREATER_EQUAL 3.13)
        target_link_options(wmain_flags INTERFACE -municode)
    else (CMAKE_VERSION VERSION_GREATER_EQUAL 3.13)
        target_link_libraries(wmain_flags INTERFACE -municode)
    endif (CMAKE_VERSION VERSION_GREATER_EQUAL 3.13)
endif (WIN32)

check_ipo_supported(RESULT LTO_SUPPORTED LANGUAGES C CXX)

if (LTO_SUPPORTED)
    message(STATUS "Link time optimization supported.")
else (LTO_SUPPORTED)
    message(STATUS "CMAKE doesn't support link time optimization for your compiler.")
endif (LTO_SUPPORTED)


# Enable testing system
enable_testing()

flags_add_optimization(default_flags)

#Subdirectories 
add_subdirectory(cpudetect)
add_subdirectory(Manual)
add_subdirectory(Pre_Processors)
add_subdirectory(Post_Processors)
add_subdirectory(Examples)

# A helper binary to generate lookup tables
add_executable(table_generator EXCLUDE_FROM_ALL pregen.cpp)
add_custom_command(
    OUTPUT tables.c
    COMMAND $<TARGET_FILE:table_generator> -irn > ${CMAKE_CURRENT_BINARY_DIR}/tables.c
    DEPENDS table_generator)

# Add bison and flex compilations
bison_target(defs ${CMAKE_CURRENT_SOURCE_DIR}/defs.y ${CMAKE_CURRENT_BINARY_DIR}/defs.c COMPILE_FLAGS -y)
flex_target(scan ${CMAKE_CURRENT_SOURCE_DIR}/scan.l ${CMAKE_CURRENT_BINARY_DIR}/scan.c)
set_source_files_properties(${BISON_defs_OUTPUTS}
    PROPERTIES OBJECT_DEPENDS ${FLEX_scan_OUTPUTS})

# Sources which are built only once
set (SRC
    dds.cpp
    dds.h
    dealer_once.cpp
    initrandom.cpp
    initrandom.h
    main.c
    pbn.c
    pbn.h
    pregen.h
    pointcount.c
    pointcount.h
    ${BISON_defs_OUTPUTS}
    tables.c
    tree.h
    )

if (NOT GETOPT_SUPPORTED)
    list(APPEND SRC getopt.c)
endif (NOT GETOPT_SUPPORTED)

# Build sources for different compiler optimization parameters
set(MV_SRC
    card.h
    c4.cpp
    c4.h
    dealer.cpp
    dealer.h
    shuffle.cpp
    shuffle.h
    )

dealer_add_executable(dealer ${SRC})
# Required libraries
dealer_target_link_libraries(dealer cpudetect ${CMAKE_DL_LIBS} default_flags wmain_flags)
if (LTO_SUPPORTED)
    dealer_set_target_properties(dealer PROPERTIES INTERPROCEDURAL_OPTIMIZATION TRUE)
endif ()

# Add multi versioned objects to the source list
foreach (CFG ${COMPILER_CONFIGS})
    dealer_add_library(dealer_${CFG} OBJECT EXCLUDE_FROM_ALL ${MV_SRC})
    dealer_target_compile_definitions(dealer_${CFG} PRIVATE MVDEFAULT=${CFG} MV${CFG}=1)
    dealer_target_link_libraries(dealer dealer_${CFG})
    dealer_target_compile_options(dealer_${CFG} PRIVATE ${OPTIMIZE_${CFG}_CPP_FLAGS})
    if (LTO_SUPPORTED)
        dealer_set_target_properties(dealer_${CFG} PROPERTIES INTERPROCEDURAL_OPTIMIZATION TRUE)
    endif ()
endforeach (CFG ${COMPILER_CONFIGS})

add_executable(dealergenlib genddslib.cpp)

if (WIN32)
    dealer_target_link_libraries(dealer ws2_32)
    target_link_libraries(dealergenlib ws2_32)
endif (WIN32)
target_link_libraries(dealergenlib default_flags)

# Make sure generated sources can find headers
dealer_target_include_directories(dealer PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

# Make sure submodules are available correctly
if (SUBMODULES)
    set_source_files_properties(dds.cpp
        PROPERTIES OBJECT_DEPENDS submodule)
    set_source_files_properties(shuffle.cpp
        PROPERTIES OBJECT_DEPENDS submodule)
    set_source_files_properties(genddslib.cpp
        PROPERTIES OBJECT_DEPENDS submodule)
    set_source_files_properties(pregen.cpp
        PROPERTIES OBJECT_DEPENDS submodule)
endif (SUBMODULES)

#Install binaries
install(TARGETS dealer dealergenlib
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

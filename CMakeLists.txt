cmake_minimum_required(VERSION 3.12)

# Make sure cmake finds custom modules
list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

# Use new policy for version number variables
cmake_policy(SET CMP0048 NEW)

# Declare project information.
project(dealer
    VERSION 0.99.0
    DESCRIPTION "Generates bridge hands for statistical analyze"
    HOMEPAGE_URL "https://github.com/suokko/dealer"
    LANGUAGES CXX C)

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

set(UTF8_DEFAULT ON)
if (WIN32)
    set(UTF8_DEFAULT OFF)
endif (WIN32)

option(UTF8_OUTPUT "Enable utf-8 output" ${UTF8_DEFAULT})

# Set symbols hidden by default to eliminate unnecessary exported symbols
cmake_policy(SET CMP0063 NEW)
set(CMAKE_CXX_VISIBILITY_PRESET internal)
set(CMAKE_VISIBILITY_INLINES_HIDDEN 1)

# Find required dependencies
find_package(BISON REQUIRED)
find_package(FLEX REQUIRED)
find_package(Perl QUIET)

#import cmake helpers
include(GNUInstallDirs)
include(try_compile_cached)
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

# Enable testing system
enable_testing()

# Check for architecture optimization flag support
# @TODO: This needs compiler specific defaults
function(do_optimization_flags _NAME CPP_FLAGS FLAG_HELP)
    list(LENGTH ARGN _LEN)
    if (_LEN GREATER 0)
        list(GET ARGN 0 OPTION_HELP)
    endif (_LEN GREATER 0)
    if (_LEN GREATER 1)
    list(GET ARGN 1 OPTION_DEFAULT)
    endif (_LEN GREATER 1)
    set(OPTIMIZE_${_NAME}_CPP_FLAGS ${CPP_FLAGS}
        CACHE STRING ${FLAG_HELP})
    mark_as_advanced(OPTIMIZE_${_NAME}_CPP_FLAGS)

    if (NOT FLAGS_${_NAME} STREQUAL OPTIMIZE_${_NAME}_CPP_FLAGS)
        unset(OPTIMIZE_${_NAME}_C CACHE)
        unset(OPTIMIZE_${_NAME}_CXX CACHE)
        set(FLAGS_${_NAME} ${OPTIMIZE_${_NAME}_CPP_FLAGS} CACHE INTERNAL "" FORCE)
    endif (NOT FLAGS_${_NAME} STREQUAL OPTIMIZE_${_NAME}_CPP_FLAGS)

    try_compiler_flag(C "${OPTIMIZE_${_NAME}_CPP_FLAGS}" OPTIMIZE_${_NAME}_C)
    try_compiler_flag(CXX "${OPTIMIZE_${_NAME}_CPP_FLAGS}" OPTIMIZE_${_NAME}_CXX)
    if (OPTIMIZE_${_NAME}_C AND OPTIMIZE_${_NAME}_CXX)
#@TODO: Make source compilation works correctly with some optimization passes missing
#        if (OPTION_HELP)
#            option(OPTIMIZE_${_NAME} ${OPTION_HELP} ${OPTION_DEFAULT})
#        else (OPTION_HELP AND OFF)
            set(OPTIMIZE_${_NAME} ON PARENT_SCOPE)
#        endif (OPTION_HELP AND OFF)
    else (OPTIMIZE_${_NAME}_C AND OPTIMIZE_${_NAME}_CXX)
        set(OPTIMIZE_${_NAME} OFF PARENT_SCOPE)
    endif (OPTIMIZE_${_NAME}_C AND OPTIMIZE_${_NAME}_CXX)
endfunction(do_optimization_flags)

# User configuration for optimization flags
#do_optimization_flags(NATIVE_ONLY
#    "-march=native -mtune=native"
#    "Compiler flags used to compile native only binary"
#    "Make dealer to include only native optimiation to the processor"
#    OFF)

set(COMPILER_CONFIGS default sse2 popcnt sse4 bmi2 avx2)
foreach (FLAG -mfpmath=sse -ffast-math)
    string(REGEX REPLACE "[-=]" "_" _NAME ${FLAG})
    try_compiler_flag(C ${FLAG} CHECK${_NAME}_C)
    try_compiler_flag(CXX ${FLAG} CHECK${_NAME}_CXX)
    if (CHECK${_NAME}_C AND CHECK${_NAME}_CXX)
        list(APPEND COMPILER_CONFIG_default ${FLAG})
    endif (CHECK${_NAME}_C AND CHECK${_NAME}_CXX)
endforeach ()

set(COMPILER_CONFIG_sse2 ${COMPILER_CONFIG_default} -msse -msse2)
set(COMPILER_CONFIG_popcnt ${COMPILER_CONFIG_sse2} -msse3 -mpopcnt)
set(COMPILER_CONFIG_sse4 ${COMPILER_CONFIG_popcnt} -msse4.1 -msse4.2)
set(COMPILER_CONFIG_bmi2 ${COMPILER_CONFIG_sse4} -mavx -mbmi2)
set(COMPILER_CONFIG_avx2 ${COMPILER_CONFIG_bmi2} -mavx2)

foreach (CFG IN LISTS COMPILER_CONFIGS)
    if (NOT CFG STREQUAL "default")
        do_optimization_flags(${CFG}
            "${COMPILER_CONFIG_${CFG}}"
            "Compiler flags used to compile ${CFG} optimization version"
            "Enable optimization for ${CFG} instruction set"
            ON)
    else (NOT CFG STREQUAL "default")
        do_optimization_flags(${CFG}
            "${COMPILER_CONFIG_${CFG}}"
            "Compiler flags used to compile ${CFG} optimization version")
    endif (NOT CFG STREQUAL "default")
endforeach (CFG IN LISTS COMPILER_CONFIGS)

# Make sure we only use selected configs
if (OPTIMIZE_NATIVE_ONLY)
    set(COMPILER_CONFIGS default)
    set(OPTIMIZE_default "${OPTIMIZE_default} ${OPTIMIZE_NATIVE_ONLY}")
else (OPTIMIZE_NATIVE_ONLY)

    foreach (CFG IN LISTS COMPILER_CONFIGS)
        if (OPTIMIZE_${CFG})
            list(APPEND temp_CFGS ${CFG})
        endif (OPTIMIZE_${CFG})
    endforeach (CFG IN LISTS COMPILER_CONFIGS)

    set(COMPILER_CONFIGS ${temp_CFGS})
    unset(temp_CFGS)
endif (OPTIMIZE_NATIVE_ONLY)


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
    c4.c
    c4.h
    dealer.c
    dealer.h
    shuffle.cpp
    shuffle.h
    )

dealer_add_executable(dealer ${SRC})
# Required libraries
dealer_target_link_libraries(dealer cpudetect ${CMAKE_DL_LIBS})

# Add multi versioned objects to the source list
foreach (CFG ${COMPILER_CONFIGS})
    dealer_add_library(dealer_${CFG} OBJECT EXCLUDE_FROM_ALL ${MV_SRC})
    dealer_target_compile_definitions(dealer_${CFG} PRIVATE MVDEFAULT=${CFG})
    dealer_target_link_libraries(dealer dealer_${CFG})
    dealer_target_compile_options(dealer_${CFG} PRIVATE ${OPTIMIZE_${CFG}_CPP_FLAGS})
endforeach (CFG ${COMPILER_CONFIGS})

add_executable(dealergenlib genddslib.cpp)

if (WIN32)
    dealer_target_link_libraries(dealer ws2_32)
    target_link_libraries(dealergenlib ws2_32)
endif (WIN32)

# Make sure generated sources can find headers
dealer_target_include_directories(dealer PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

#Enable utf-8 output
if (UTF8_OUTPUT)
    dealer_target_compile_definitions(dealer PRIVATE UTF8_SUPPORTED)
endif (UTF8_OUTPUT)

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

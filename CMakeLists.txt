cmake_minimum_required(VERSION 3.1)

# Make sure cmake finds custom modules
list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

#Set default build type to Release
set(CMAKE_BUILD_TYPE_INIT Release)

# Make sure compiler selects c++14 standard
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Use new policy for version number variables
cmake_policy(SET CMP0048 NEW)
set(DESCR "Generates bridge hands for statistical analyze")
set(PURL "https://github.com/suokko/dealer")
# Declare project information.
if (CMAKE_VERSION VERSION_GREATER 3.12)
    project(dealer
        VERSION 0.99.1
        DESCRIPTION ${DESCR}
        HOMEPAGE_URL ${PURL}
        LANGUAGES CXX C)
elseif (CMAKE_VERSION VERSION_GREATER 3.9)
    project(dealer
        VERSION 0.99.1
        DESCRIPTION ${DESCR}
        LANGUAGES CXX C)
    set(PROJECT_HOMEPAGE_URL ${PURL})
    set(DEALER_HOMEPAGE_URL ${PURL})
else ()
    project(dealer
        VERSION 0.99.1
        LANGUAGES CXX C)
    set(PROJECT_HOMEPAGE_URL ${PURL})
    set(DEALER_HOMEPAGE_URL ${PURL})
    set(PROJECT_DESCRIPTION ${DESCR})
    set(DEALER_DESCRIPTION ${DESCR})
endif ()

# Set symbols hidden by default to eliminate unnecessary exported symbols
if (CMAKE_VERSION VERSION_GREATER 3.3)
    cmake_policy(SET CMP0063 NEW)
endif ()
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
        COMMAND "${GIT_EXECUTABLE}" submodule update --init --recursive
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

if (NOT PERL_FOUND)
    message(WARNING "Perl wasn't found. Disabling tests. perl with module IPC::Run is required for testing.")
else ()
    execute_process(COMMAND ${PERL_EXECUTABLE} -MIPC::Run -e ""
        ERROR_QUIET RESULT_VARIABLE RES)
    if (NOT RES EQUAL "0")
        set(PERL_FOUND FALSE)
        message(WARNING "Perl module IPC::Run wasn't found. Disabling tests.")
    endif ()
endif ()

#Configure packaging
file(STRINGS "${CMAKE_SOURCE_DIR}/.gitignore" GITIGNORE)
list(REMOVE_ITEM GITIGNORE "SourceVersion.cmake")
#Convert glob to regular expression
string(REPLACE "." "\\\\." GITIGNORE "${GITIGNORE}")
string(REPLACE "*" ".*" GITIGNORE "${GITIGNORE}")
list(APPEND GITIGNORE "\\\\.git/" "\\\\.gitmodules$" "/dds/" "/pcg-cpp/")
# set default package formats which are supported in build system
set(CPACK_SOURCE_GENERATOR TGZ ZIP)
if (WIN32)
    set(CPACK_GENERATOR NSIS)
elseif (CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(CPACK_GENERATOR DEB RPM)
else ()
    message(WARNING "No default binary package formar for your system")
endif ()
set(CPACK_PACKAGE_VENDOR "Dealer project")
set(CPACK_PACKAGE_DESCRIPTION_FILE "${CMAKE_SOURCE_DIR}/DESCRIPTION")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_SOURCE_DIR}/LICENSE")
set(CPACK_SOURCE_IGNORE_FILES "${GITIGNORE}")
set(CPACK_PACKAGE_CONTACT "$ENV{NAME} <$ENV{EMAIL}>")

# DEB packaging configuration
set(CPACK_DEB_COMPONENT_INSTALL ON)
set(CPACK_DEBIAN_PACKAGE_HOMEPAGE "${PROJECT_HOMEPAGE_URL}")
set(CPACK_DEBIAN_PACKAGE_SECTION "games")
set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)
set(CPACK_DEBIAN_ENABLE_COMPONENT_DEPENDS ON)

set(CPACK_DEBIAN_RUNTIME_PACKAGE_NAME "dealer")
set(CPACK_DEBIAN_DOCS_PACKAGE_NAME "dealer-doc")

# RPM packaging configuration
set(CPACK_RPM_MAIN_COMPONENT Runtime)
set(CPACK_RPM_FILENAME RPM-DEFAULT)
set(CPACK_RPM_DOCS_PACKAGE_ARCHITECTURE noarch)

# Windows NSIS installer configuration
set(CPACK_NSIS_ENABLE_UNINSTALL_BEFORE_INSTALL OFF)
set(CPACK_NSIS_MODIFY_PATH ON)
set(CPACK_NSIS_URL_INFO_ABOUT "${PROJECT_HOMEPAGE_URL}")

set(CMAKE_INSTALL_SYSTEM_RUNTIME_COMPONENT Runtime_Libs)

#import cmake helpers
include(GNUInstallDirs)
include(ExternalProject)
include(CPack)
include(InstallRequiredSystemLibraries)

if (CMAKE_VERSION VERSION_GREATER 3.9)
    cmake_policy(SET CMP0069 NEW)
    include(CheckIPOSupported)
else ()
    message(STATUS "Disabling link time optimization for cmake below version 3.9")
endif ()

include(oldcmake)
include(try_compile_cached)
include(optimization_flags)
include(coverage)
include(pod2man)
include(dealer_version)

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
    message(SEND_ERROR "Compiler is missing std::thread support. If you use mingw then selecting alternative threading model potentially fixes the issue")
endif (NOT STD_THREAD_SUPPORTED)

# Check if getopt is supported
try_compile_cached(CXX GETOPT_SUPPORTED
    SOURCE_TEXT
"#include <unistd.h>
int main(int argc, char* const argv[]) {
    const char *opt = \"h\";
    return getopt(argc, argv, opt);
}")

add_library(wmain_flags INTERFACE)
if (WIN32)
    # Check if calling from wmain to main works
    try_run_cached(CXX CHECK_WMAIN_SUPPORTED
        SOURCE_TEXT "int rv = 1;
int main(void) {return rv;}
int wmain(void) {rv = 0; return main();}"
        COMPILE_DEFINITIONS -municode
        LINK_LIBRARIES -municode)

    if (NOT CHECK_WMAIN_SUPPORTED)
        message(SEND_ERROR "Failed to use wmain entry for windows.")
    endif ()

    target_compile_options(wmain_flags INTERFACE -municode)
    target_link_options_old(wmain_flags INTERFACE -municode)
endif (WIN32)

if (CMAKE_VERSION VERSION_GREATER 3.9)
    check_ipo_supported(RESULT LTO_CHECK_SUPPORTED LANGUAGES CXX)

    if (LTO_CHECK_SUPPORTED)
        message(STATUS "Link time optimization supported.")
        option(LTO_SUPPORTED "Support link time optimization" ON)
    else (LTO_CHECK_SUPPORTED)
        message(STATUS "CMAKE doesn't support link time optimization for your compiler.")
    endif (LTO_CHECK_SUPPORTED)
endif ()


# Enable testing system
enable_testing()

#Check if submodules are available from the system
set(DDSREQUIRED)
if (NOT SUBMODULES)
    set(DDSREQUIRED REQUIRED)
endif ()
set(PCGREQUIRED)
if (NOT SUBMODULES)
    set(PCGREQUIRED REQUIRED)
endif ()

find_package(Boost REQUIRED)
if (Boost_FOUND AND NOT TARGET Boost::boost)
    add_library(Boost::boost IMPORTED INTERFACE)
    target_include_directories(Boost::boost INTERFACE ${Boost_INCLUDE_DIRS})
endif ()

find_package(DDS ${DDSREQUIRED})
find_package(PCGCXX ${PCGREQUIRED})

if (NOT USE_SYSTEM_DDS)
    message(STATUS "System libdds wasn't found. Using source from dds subdirectory.")
    # Make sure submodules has been updated
    execute_process(COMMAND "${GIT_EXECUTABLE}" submodule update --init --recursive
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
    set(TOOL)
    if (CMAKE_CROSSCOMPILING)
        set(TOOL CMAKE_CACHE_ARGS "-DCMAKE_TOOLCHAIN_FILE:FILEPATH=${CMAKE_TOOLCHAIN_FILE}")
    endif ()
    ExternalProject_Add(dds
        SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/dds"
        CMAKE_CACHE_ARGS "-DCMAKE_INSTALL_PREFIX:PATH=${CMAKE_CURRENT_BINARY_DIR}/dds-install"
        CMAKE_CACHE_ARGS "-DCMAKE_CXX_COMPILER:FILEPATH=${CMAKE_CXX_COMPILER}"
        ${TOOL}
        STAMP_DIR dds-prefix/stamp
        BINARY_DIR dds-prefix/build
        INSTALL_DIR dds-install
        STEP_TARGETS configure build test
        )

    add_library(DDS INTERFACE)
    target_include_directories(DDS INTERFACE
        ${CMAKE_CURRENT_BINARY_DIR}/dds-install/include/dds)
    if (WIN32)
        set(DDS_LIBDIR ${CMAKE_CURRENT_BINARY_DIR}/dds-install/bin)
    else ()
        set(DDS_LIBDIR ${CMAKE_CURRENT_BINARY_DIR}/dds-install/lib)
    endif ()
    target_link_directories_old(DDS INTERFACE ${DDS_LIBDIR})
    if (WIN32)
        set(WINE)
        if (CMAKE_CROSSCOMPILING)
            set(WINE "WINE")
        endif ()
        message(WARNING "You likely need to add to your ${WINE}PATH when \
running dealer from the build directory. This is required to allow library \
loading to find the libdds.dll file.
set PATH=%PATH%;${DDS_LIBDIR}")
    endif ()

    set(DDS_INSTALL_ARGS
        --install dds-prefix/build
        --prefix "\${CMAKE_INSTALL_PREFIX}"
        --component Runtime
        )
    install(CODE
        "execute_process(COMMAND ${CMAKE_COMMAND} ${DDS_INSTALL_ARGS})"
        COMPONENT Runtime
        )

endif (NOT USE_SYSTEM_DDS)

flags_add_optimization(default_flags)
configure_version(version.cpp.in.in)

#Subdirectories 
add_subdirectory(cpudetect)
add_subdirectory(Manual)
add_subdirectory(Pre_Processors)
add_subdirectory(Post_Processors)
add_subdirectory(Examples)
if (NOT GETOPT_SUPPORTED)
    add_subdirectory(getopt)
endif (NOT GETOPT_SUPPORTED)

# Add bison and flex compilations
bison_target(defs ${CMAKE_CURRENT_SOURCE_DIR}/defs.y
    ${CMAKE_CURRENT_BINARY_DIR}/defs.cpp
    COMPILE_FLAGS -y)
flex_target(scan ${CMAKE_CURRENT_SOURCE_DIR}/scan.l
    ${CMAKE_CURRENT_BINARY_DIR}/scan.cpp)
set_source_files_properties(${BISON_defs_OUTPUTS}
    PROPERTIES OBJECT_DEPENDS ${FLEX_scan_OUTPUTS})

# Sources which are built only once
set (SRC
    dds.cpp
    dds.h
    dealer_once.cpp
    main.cpp
    pbn.cpp
    pbn.h
    pregen.cpp
    pregen.h
    pointcount.cpp
    pointcount.h
    ${BISON_defs_OUTPUTS}
    tree.h
    version.cpp
    version.h
    )

# Build sources for different compiler optimization parameters
set(MV_SRC
    bittwiddle.cpp
    bittwiddle.h
    card.h
    c4.cpp
    c4.h
    dealer.cpp
    dealer.h
    shuffle.cpp
    shuffle.h
    uniform_int.h
    )

dealer_add_executable(dealer ${SRC})
# Required libraries
dealer_target_link_libraries(dealer
    cpudetect
    ${CMAKE_DL_LIBS}
    DDS
    PCGCXX
    default_flags
    wmain_flags
    )
if (LTO_SUPPORTED)
    dealer_set_target_properties(dealer PROPERTIES INTERPROCEDURAL_OPTIMIZATION TRUE)
endif ()

# Add multi versioned objects to the source list
foreach (CFG ${COMPILER_CONFIGS})
    dealer_add_library(dealer_${CFG} OBJECT EXCLUDE_FROM_ALL ${MV_SRC})
    dealer_target_compile_definitions(dealer_${CFG} PRIVATE MVDEFAULT=${CFG} MV${CFG}=1)
    # Make sure pcg headers are found during compile
    dealer_target_link_libraries(dealer_${CFG} PCGCXX)
    # Make config part of binary (just appends objects to link command)
    dealer_target_link_libraries(dealer dealer_${CFG} Boost::boost)
    dealer_target_include_directories(dealer_${CFG} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/cpudetect)
    dealer_target_compile_options(dealer_${CFG} PRIVATE ${OPTIMIZE_${CFG}_CPP_FLAGS})
    if (LTO_SUPPORTED)
        dealer_set_target_properties(dealer_${CFG} PROPERTIES INTERPROCEDURAL_OPTIMIZATION TRUE)
    endif ()
endforeach (CFG ${COMPILER_CONFIGS})

add_executable(dealergenlib genddslib.cpp)

if (WIN32)
    dealer_target_link_libraries(dealer ws2_32)
    target_link_libraries_old(dealergenlib ws2_32)
endif (WIN32)
target_link_libraries_old(dealergenlib PCGCXX default_flags)

if (NOT GETOPT_SUPPORTED)
    dealer_target_link_libraries(dealer getopt)
    target_link_libraries_old(dealergenlib getopt)
endif (NOT GETOPT_SUPPORTED)

# Make sure generated sources can find headers
dealer_target_include_directories(dealer PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

# Make sure submodules are available correctly
if (SUBMODULES)
    if (NOT USE_SYSTEM_PCGCXX)
        set_source_files_properties(shuffle.h
            PROPERTIES OBJECT_DEPENDS submodule)
        set_source_files_properties(genddslib.cpp
            PROPERTIES OBJECT_DEPENDS submodule)
    endif ()
    if (NOT USE_SYSTEM_DDS)
        set_source_files_properties(dds.cpp
            PROPERTIES OBJECT_DEPENDS dds)
    endif ()
endif (SUBMODULES)

#Install binaries
install(TARGETS dealer dealergenlib
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        COMPONENT Runtime
    )

if(MINGW)
    find_program(OBJDUMP_COMMAND objdump)
    if (OBJDUMP_COMMAND)
        # Some black magic to pull or mingw dlls into installer
        # Basic idea is to list all linked dlls using objdump. Next step is
        # to use find_path to check all linked dlls which are inside mingw
        # default library path.
        # libdds.dll is use for linked libraries because it has strict super
        # set of dependencies to dealer.exe
        cmake_policy(SET CMP0087 NEW)
        if (NOT DDS_TARGET)
            set(DDS_TARGET dealer)
        endif ()
        execute_process(COMMAND "${CMAKE_CXX_COMPILER}" -print-search-dirs
            OUTPUT_VARIABLE OUT)
        string(REGEX REPLACE ".*libraries: =" "" LIBPATH "${OUT}")
        string(REPLACE ":" ";" LIBPATH ${LIBPATH})
        install(CODE
    "
    execute_process(COMMAND \"${OBJDUMP_COMMAND}\" --private-headers \"$<TARGET_FILE:${DDS_TARGET}>\"
        OUTPUT_VARIABLE OUT)
    string(REGEX MATCHALL \"[A-Za-z0-9+-_]*\\\\.dll\" DLL_LIST \"\${OUT}\")
    foreach (F \${DLL_LIST})
        find_path(\${F}_RES \${F} PATHS ${LIBPATH})
        if (\${F}_RES)
            file(INSTALL DESTINATION \"\${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_BINDIR}\"
                TYPE SHARED_LIBRARY FILES \"\${\${F}_RES}/\${F}\")
        endif ()
    endforeach ()
    "
            COMPONENT Runtime_Libs
            )

    else ()
        message(WARNING "objdump not found. objdump is required by install target to collect required mingw dlls")
    endif ()
endif()

# Configure installation components
set(RLIBS)
if (WIN32)
    # Can this be made separated download which can be same for multiple versions?
    set(RLIBS DEPENDS Runtime_Libs)
    cpack_add_component(Runtime_Libs
        DISPLAY_NAME dealer-libs
        DESCRIPTION "Libraries required to run the dealer."
        HIDDEN
        )
endif ()
cpack_add_component(Runtime
    DISPLAY_NAME dealer
    DESCRIPTION "The dealer program to generate hands."
    ${RLIBS}
    REQUIRED)
cpack_add_component(Docs
    DISPLAY_NAME dealer-doc
    DESCRIPTION "The documentation for the dealer program."
    DEPENDS Runtime
    )

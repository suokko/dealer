

dealer_add_library(getopt EXCLUDE_FROM_ALL OBJECT
    getopt.c
    getopt.h
    )

dealer_target_include_directories(getopt PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

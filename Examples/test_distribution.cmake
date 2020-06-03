# Simple helper to run distribution tests with shell redirection
# This could be expanded to reimplement perl part of code. Reimplementing perl
# part in cmake would allow dropping the soft dependency from perl. But first
# shape function parser would need to be expanded to handle all Pre_Processors
# features.

execute_process(COMMAND ${CMD} -s 1 Test.distribution
    COMMAND ./convert.pl
    COMMAND ${DIFF_COMMAND} -u Refer.distribution -
    RESULT_VARIABLE RES
    OUTPUT_VARIABLE OUT
    ERROR_VARIABLE OUT)

if (NOT RES EQUAL 0)
    message(FATAL_ERROR ${ERR})
else (NOT RES EQUAL 0)
    message(STATUS ${OUT})
endif (NOT RES EQUAL 0)


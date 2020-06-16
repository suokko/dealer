# Simple helper to run distribution tests with shell redirection
# This could be expanded to reimplement perl part of code. Reimplementing perl
# part in cmake would allow dropping the soft dependency from perl. But first
# shape function parser would need to be expanded to handle all Pre_Processors
# features.

execute_process(COMMAND ${CMD} -s 1 Test.distribution
    COMMAND ./convert.pl
    COMMAND ${DIFF_COMMAND} -u Refer.distribution -
    OUTPUT_VARIABLE OUT
    ERROR_VARIABLE OUT)

if (OUT MATCHES "/\*\*\*FAILED\*\*\*/")
    message(FATAL_ERROR ${OUT})
else ()
    message(STATUS ${OUT})
endif ()


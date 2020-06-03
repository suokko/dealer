set(EXAMPLES
    Descr.6c
    Descr.blue_team
    Descr.controls
    Descr.dds.evalcontract
    Descr.dds.test_lead
    Descr.dds.test_lead_no_cards_defined
    Descr.dds.test_tricks
    Descr.evaluation
    Descr.hm1dr2d
    Descr.junkstayman
    Descr.notrump
    Descr.pbn
    Descr.pbn_no_set_vuln
    Descr.stayman
    Descr.test_abs
    Descr.test_altcount
    Descr.test_altcount_large
    Descr.test_altcount_neg
    Descr.test_arminuns
    Descr.test_average
    Descr.test_cccc
    Descr.test_comment
    Descr.test_defaults
    Descr.test_defaults_no_print
    Descr.test_dealer
    Descr.test_double_generate
    Descr.test_double_produce
    Descr.test_exhaust_only_one_known
    Descr.test_exhaust_two_known
    Descr.test_frequency2d
    Descr.test_frequency_no_parenthes
    Descr.test_generate_qstring
    Descr.test_generate_string
    Descr.test_generate_variable
    Descr.test_imps
    Descr.test_library
    Descr.test_multiline_string
    Descr.test_only_exression
    Descr.test_operators
    Descr.test_packorder_random
    Descr.test_pointcount_aliases
    Descr.test_pointcount_too_many
    # bias deal impossible error checking is expected to fail for now
    #    Descr.test_predeal_bias_impossible
    Descr.test_predeal_bias_player_under
    Descr.test_predeal_bias_suit_under
    Descr.test_predeal_card_twice
    Descr.test_predeal_more_than_13_cards
    Descr.test_predeal_more_than_13_hearts
    Descr.test_predeal_more_than_13_specific
    Descr.test_predeal_more_than_bias
    Descr.test_predeal_suit
    Descr.test_predeal_suit_already_set
    Descr.test_predeal_suit_too_long
    Descr.test_predeal_suit_too_long_suit
    Descr.test_predeal_west
    Descr.test_printcompact
    Descr.test_printes
    Descr.test_printes_empty
    Descr.test_score
    Descr.test_score_lower_case_contract
    Descr.test_shape_parsing
    Descr.test_swapping
    Descr.test_too_many_cards
    Descr.test_unknown_variable
    Descr.test_variable_redefination
    Descr.test_wrong_number_of_cards
    Descr.vvga2
    Descr.weaktwo
    Descr.yarb
    Descr.you_hold
    )

install(FILES ${EXAMPLES}
    DESTINATION ${CMAKE_INSTALL_DOCDIR}/Examples)

if (PERL_FOUND)
    include(ProcessorCount)
    ProcessorCount(CORES)
    set(ctest_args --output-on-failure)
    if (NOT CORES EQUAL 0)
        set(ctest_args ${ctest_args} -j${CORES})
    endif (NOT CORES EQUAL 0)
    # Add custom target for testing to have better control what gets tested
    # Check runs test with current build configuration as is
    # @TODO: Figure out how to run this correctly from multi configuration generator
    add_custom_target(check
        COMMAND ${CMAKE_CTEST_COMMAND} -L "regular" ${ctest_args}
        USES_TERMINAL)
    add_dependencies(check dealer)
    # check_coverage runs test using coverage built and generates html pages for
    # coverage report.
    add_custom_target(check_coverage_run
        COMMAND ${CMAKE_CTEST_COMMAND} -L "coverage" ${ctest_args}
        USES_TERMINAL) 
    add_dependencies(check_coverage_run dealer.cov)

    # Built html pages from coverage data
    find_program(LCOV_COMMAND lcov)
    mark_as_advanced(LCOV_COMMAND)
    find_program(GENHTML_COMMAND genhtml)
    mark_as_advanced(GENHTML_COMMAND)
    find_program(OPEN_COMMAND NAMES open xdg-open)
    mark_as_advanced(OPEN_COMMAND)

    set(COVERAGE_DIR "coverage")
    set(DEV_NULL > /dev/null 2> /dev/null)
    if (WIN32)
        set(DEV_NULL > NUL 2> NUL)
    endif (WIN32)

    if (CMAKE_CXX_COMPILER_ID MATCHES "GNU")
        if (LCOV_COMMAND AND GENHTML_COMMAND)
            # @TODO: Make sure clean can remove all generate files
            add_custom_target(check_coverage
                COMMAND ${CMAKE_COMMAND} -E make_directory ${COVERAGE_DIR}
                COMMAND ${LCOV_COMMAND} --base-directory . --directory . --rc lcov_branch_coverage=1     -q -c -o ${COVERAGE_DIR}/dealer.info
                COMMAND ${LCOV_COMMAND} --rc lcov_branch_coverage=1 -q -o ${COVERAGE_DIR}/dealer.stripped.info --remove ${COVERAGE_DIR}/dealer.info '/usr*'
                COMMAND ${GENHTML_COMMAND} --num-spaces 4 -o ${COVERAGE_DIR} --branch-coverage -t "Dealer test coverage" ${COVERAGE_DIR}/dealer.stripped.info
                COMMAND ${OPEN_COMMAND} ${COVERAGE_DIR}/index.html ${DEV_NULL}
                WORKING_DIRECTORY ${PROJECT_BINARY_DIR})
        else (${LCOV_COMMAND} AND ${GENHTML_COMMAND})
            set(ERRMSG "Coverage report requires")
            if (NOT LCOV_COMMAND)
                set(ERRMSG "${ERRMSG} lcov")
                if (NOT GENHTML_COMMAND)
                    set(ERRMSG "${ERRMSG} and")
                endif (NOT GENHTML_COMMAND)
            endif (NOT LCOV_COMMAND)
            if (NOT GENHTML_COMMAND)
                set(ERRMSG "${ERRMSG} genhtml")
            endif (NOT GENHTML_COMMAND)
            set(ERRMSG "${ERRMSG} which are required for coverage report generation")

            add_custom_target(check_coverage
                COMMAND ${CMAKE_COMMAND} -E echo ${ERRMSG})
        endif (LCOV_COMMAND AND GENHTML_COMMAND)
    else (CMAKE_CXX_COMPILER_ID MATCHES "GNU")
        add_custom_target(check_coverage
            COMMAND ${CMAKE_COMMAND} -E echo "Examples/CMakeLists.txt doesn't have support to generate coverage report for your compiler")
    endif (CMAKE_CXX_COMPILER_ID MATCHES "GNU")

    # Make sure test are run before report generation
    add_dependencies(check_coverage check_coverage_run)

    find_program(DIFF_COMMAND diff)

    add_test(NAME test_distribution
        COMMAND ${CMAKE_COMMAND} -D CMD=$<TARGET_FILE:dealer> -D DIFF_COMMAND=${DIFF_COMMAND}
            -P test_distribution.cmake
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
    set_tests_properties(test_distribution PROPERTIES LABELS "regular")

    add_test(NAME test_distribution_cov
        COMMAND ${CMAKE_COMMAND} -D CMD=$<TARGET_FILE:dealer.cov> -D DIFF_COMMAND=${DIFF_COMMAND}
            -P ${CMAKE_CURRENT_SOURCE_DIR}/test_distribution.cmake
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
    set_tests_properties(test_distribution_cov PROPERTIES LABELS "coverage")

    foreach (SRC ${EXAMPLES})
        add_test(NAME test_${SRC}
            COMMAND ${PERL_COMMAND} test_dealer.pl $<TARGET_FILE:dealer> ${SRC}
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
        set_tests_properties(test_${SRC} PROPERTIES LABELS "regular")

        add_test(NAME test_${SRC}_cov
            COMMAND ${PERL_COMMAND} test_dealer.pl $<TARGET_FILE:dealer.cov> ${SRC}
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
        set_tests_properties(test_${SRC}_cov PROPERTIES LABELS "coverage")
    endforeach (SRC ${EXAMPLES})
elseif (NOT PERL_FOUND)
    # Make targets showing error that perl wasn't found if trying to run tests.
    set(PERL_MISSING_ERROR "Perl wasn't found during cmake configure. Perl is required to run tests.")
    add_custom_target(check
        COMMAND ${CMAKE_COMMAND} -E echo ${PERL_MISSING_ERROR}
        COMMAND false)
    add_custom_target(check_coverage
        COMMAND ${CMAKE_COMMAND} -E echo ${PERL_MISSING_ERROR}
        COMMAND false)
endif (PERL_FOUND)

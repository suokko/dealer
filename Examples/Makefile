#
# Makefile to process all Descr.* files with dealer and write the output
# to Output.*.  make test executes the test, make clean cleans up.
# Depending on the CPU power that you have, these test may take up to
# 30 minutes to run.
#

include ${CURDIR}/Makefile.vars
SRCDIR := ${CURDIR}/Examples

SHELL=/bin/sh

OUTPUT = $(wildcard ${SRCDIR}/Output.*)

test: makehtml

distribution: ${PROGRAMCOV} clearstats
	@cd ${SRCDIR} && ../${PROGRAMCOV} -s 1 <Test.distribution | ./convert.pl >Output.distribution && diff Output.distribution Refer.distribution

examples: ${PROGRAMCOV} clearstats
	@cd ${SRCDIR} && ./test_dealer.pl ${PROGRAMCOV}

refer:
	-for f in ${OUTPUT}; do \
		echo $$f; \
		f1=`echo $$f | sed s/Output/Refer/` ; \
		echo $$f1; \
		/bin/cp $$f $$f1; \
	done 

makehtml: examples distribution 
	${SMKDIR} .test
	@echo "  LCOV   process" && lcov --base-directory . --directory . --rc lcov_branch_coverage=1 -c -o .test/dealer.info
	@echo "  LCOV   strip" && lcov --remove .test/dealer.info "/usr*" --rc lcov_branch_coverage=1 -o .test/dealer.info
	@echo "  GENHTML" && genhtml --num-spaces 4 -o .test/ --branch-coverage -t "Dealer test coverage" .test/dealer.info

testresults: makehtml
	@echo "  XDGOPEN" && xdg-open .test/index.html

clearstats:
	@echo "  LCOV   clear" && lcov --base-directory . --directory . --zerocounters -q

exampleclean:
	-rm -f ${OUTPUT}

clean: exampleclean

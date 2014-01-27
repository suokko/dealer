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

test: examples distribution 

distribution: ${PROGRAM}
	@cd ${SRCDIR} && ../dealer -s 1 <Test.distribution | ./convert.pl >Output.distribution && diff Output.distribution Refer.distribution

examples: ${PROGRAM}
	@cd ${SRCDIR} && ./test_dealer.pl

refer:
	-for f in ${OUTPUT}; do \
		echo $$f; \
		f1=`echo $$f | sed s/Output/Refer/` ; \
		echo $$f1; \
		/bin/cp $$f $$f1; \
	done 


exampleclean:
	-rm -f ${OUTPUT}

clean: exampleclean

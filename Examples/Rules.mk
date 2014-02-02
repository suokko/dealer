
#
# Makefile to process all Descr.* files with dealer and write the output
# to Output.*.  make test executes the test, make clean cleans up.
# Depending on the CPU power that you have, these test may take up to
# 30 minutes to run.
#

SRCDIR := ${CURDIR}/Examples

SHELL=/bin/sh

OUTPUT = $(wildcard ${SRCDIR}/Output.*)
DESCR := $(wildcard ${SRCDIR}/Descr.*)
# move the Descr.test_dealer to front because it is slow
DESCR := ${SRCDIR}/Descr.test_dealer $(filter-out .test_dealer,${DESCR})
DDSEXISTS := $(shell which dds)
LCOVEXISTS := $(shell which lcov)
ifeq (${DDSEXISTS},)
DESCR := $(filter-out .dds.,${DESCR})
$(info *********************************************************)
$(info * dds is optional tool but strongly encouraged to have. *)
$(info *********************************************************)
endif
ifeq ($(LCOVEXISTS),)
$(info **********************************************************)
$(info * lcov is optional tool but strongly encouraged to have. *)
$(info **********************************************************)
endif

ifeq ($(filter cov,${MAKECMDGOALS}),cov)
PROG:=${PROGRAMCOV}
else
ifeq ($(filter release,${MAKECMDGOALS}),release)
PROG:=${PROGRAM}.prof
else
PROG:=${PROGRAM}
endif
endif

OUT = $(subst Descr.,Output.,${DESCR})

test: distribution examples 
cov: makehtml

ALLTESTS := distribution examples

distribution: ${PROG} clearstats
	@cd ${SRCDIR} && ../${PROG} -s 1 <Test.distribution | ./convert.pl >Output.distribution && diff Output.distribution Refer.distribution

examples: ${OUT}

${SRCDIR}/Output.%: ${SRCDIR}/Descr.% ${PROG} clearstats
	@-cd ${SRCDIR} && ./test_dealer.pl ${PROG} $(notdir $<)

refer:
	-for f in ${OUTPUT}; do \
		echo $$f; \
		f1=`echo $$f | sed s/Output/Refer/` ; \
		echo $$f1; \
		/bin/cp $$f $$f1; \
		[ -e $$f.err ] && /bin/cp $$f.err $$f1.err; \
	done 

makehtml: examples distribution 
	${SMKDIR} .test
	@echo "  LCOV   process" && lcov --base-directory . --directory . --rc lcov_branch_coverage=1 -q -c -o .test/dealer.info
	@echo "  LCOV   strip" && lcov --remove .test/dealer.info "/usr*" --rc lcov_branch_coverage=1 -q -o .test/dealer.info
	@echo "  GENHTML" && genhtml --num-spaces 4 -o .test/ --branch-coverage -t "Dealer test coverage" .test/dealer.info

testresults: makehtml
	@echo "  XDGOPEN" && xdg-open .test/index.html

clearstats:
	@echo "  LCOV   clear" && lcov --base-directory . --directory . --zerocounters -q
	@echo "  RM     *.gcda" && rm -f ${BUILDDIR}/*.gcda

exampleclean:
	-rm -f ${OUTPUT}

clean: exampleclean
include ${CURDIR}/Makefile.vars

SRCDIR := ${CURDIR}/Random

RNDOBJ      := $(addprefix ${BUILDDIR}/, $(notdir $(subst .c,.o,$(wildcard ${SRCDIR}/*.c))))
RNDOBJCOV   = ${subst .o,.cov.o, ${RNDOBJ}}
RNDLIB      = ${BUILDDIR}/libgnurand.a
RNDLIBCOV      = ${subst .a,.cov.a, ${RNDLIB}}

rndclean:
	-rm ${RNDOBJ} ${RNDLIB} ${subst .o,.d,${RNDOBJ}}

clean: rndclean

${BUILDDIR}/%.o: ${SRCDIR}/%.c ${BUILDDIR}/deb
	${SCC} -c ${CFLAGS} -o $@ $<

${BUILDDIR}/%.cov.o: ${SRCDIR}/%.c ${BUILDDIR}/deb
	${SCC} -c ${CFLAGS} ${COVFLAGS} -o $@ $<

${RNDLIB}: ${RNDOBJ}
	${SAR} ${ARFLAGS} $@ $?

${RNDLIBCOV}: ${RNDOBJCOV}
	${SAR} ${ARFLAGS} $@ $?

${PROGRAM}: ${RNDLIB}
${PROGRAMCOV}: ${RNDLIBCOV}

-include $(RNDOBJS:.o=.d)

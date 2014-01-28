include ${CURDIR}/Makefile.vars

SRCDIR := ${CURDIR}/Random

RNDOBJ      := $(addprefix ${BUILDDIR}/, $(notdir $(subst .c,.o,$(wildcard ${SRCDIR}/*.c))))
RNDOBJCOV   = ${subst .o,.cov.o, ${RNDOBJ}}
RNDLIB      = ${BUILDDIR}/libgnurand.a
RNDLIBCOV      = ${subst .a,.cov.a, ${RNDLIB}}

rndclean:
	@rm -f ${RNDOBJ} ${BUILDDIR}/main.o ${BUILDDIR}/main.d ${RNDOBJCOV} ${RNDLIB} ${subst .o,.d,${RNDOBJ} ${RNDOBJCOV}}

clean: rndclean

${BUILDDIR}/%.o: ${SRCDIR}/%.c ${BUILDDIR}/deb
	${SCC} -c ${CFLAGS} -o $@ $<

${BUILDDIR}/%.cov.o: ${SRCDIR}/%.c ${BUILDDIR}/deb
	${SCC} -c ${CFLAGS} ${COVFLAGS} -o $@ $<

${RNDLIB}: ${RNDOBJ}
	${SAR} ${ARFLAGS} $@ $^

${RNDLIBCOV}: ${RNDOBJCOV}
	${SAR} ${ARFLAGS} $@ $^

${PROGRAM}: ${RNDLIB}
${PROGRAMCOV}: ${RNDLIBCOV}

-include $(RNDOBJS:.o=.d)

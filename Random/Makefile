include ${CURDIR}/Makefile.vars

SRCDIR := ${CURDIR}/Random

RNDOBJ      := $(addprefix ${BUILDDIR}/, $(notdir $(subst .c,.o,$(wildcard ${SRCDIR}/*.c))))
RNDLIB      = ${BUILDDIR}/libgnurand.a

rndclean:
	-rm ${RNDOBJ} ${RNDLIB} ${subst .o,.d,${RNDOBJ}}

clean: rndclean

${BUILDDIR}/%.o: ${SRCDIR}/%.c ${BUILDDIR}/deb
	${SCC} -c ${CFLAGS} -o $@ $<

${RNDLIB}: ${RNDOBJ}
	${SAR} ${ARFLAGS} $@ $?

${PROGRAM}: ${RNDLIB}

-include $(RNDOBJS:.o=.d)

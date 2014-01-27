
FLEX    = flex
YACC    = yacc

TARFILE  = ${PROGRAM}.tar
GZIPFILE = ${PROGRAM}.tar.gz

include ${PWD}/Makefile.vars

LSRC = scan.l
YSRC = defs.y
HDR  = dealer.h tree.h
LOBJ = ${BUILDDIR}/scan.c
YOBJ = ${BUILDDIR}/defs.c
SRC  = dealer.c pbn.c  c4.c getopt.c pointcount.c

OBJ  = ${addprefix ${BUILDDIR}/, dealer.o defs.o pbn.o c4.o pointcount.o}
OBJCOV = ${subst .o,.cov.o,${OBJ}}

${PROGRAM}: ${OBJ}
	$(SCC) ${CFLAGS} -o $@ $?

${PROGRAMCOV}: ${OBJCOV}
	$(SCC) ${CFLAGS} ${COVFLAGS} -o $@ $?

clean:
	@rm -f ${OBJ} ${LOBJ} ${YOBJ} ${subst .o,.d,${OBJ}} ${BUILDDIR}/deb dealer
	@[ -e ${BUILDDIR} ] && rmdir ${BUILDDIR} || true

tarclean: clean ${YOBJ}
	rm -f dealer
	rm -f ${TARFILE}  ${GZIPFILE}

tarfile: tarclean
	cd .. ; \
	tar cvf ${TARFILE} ${PROGRAM} --exclude CVS --exclude ${TARFILE}; \
	mv ${TARFILE} ${PROGRAM} 
	gzip -f ${TARFILE}

#
# Lex
#
${BUILDDIR}/%.c: %.l ${BUILDDIR}/deb
	${SFLEX} -o $@ $<

#
# Yacc
#
${BUILDDIR}/%.c: %.y ${BUILDDIR}/deb
	${SYACC} $< -o $@

#
# C-code
#
${BUILDDIR}/%.o: %.c ${BUILDDIR}/deb
	${SCC} -c ${CFLAGS} -o $@ $<

${BUILDDIR}/%.o: ${BUILDDIR}/%.c ${BUILDDIR}/deb
	${SCC} -c ${CFLAGS} -o $@ $<

${BUILDDIR}/%.cov.o: %.c ${BUILDDIR}/deb
	${SCC} -c ${CFLAGS} ${COVFLAGS} -o $@ $<

${BUILDDIR}/%.cov.o: ${BUILDDIR}/%.c ${BUILDDIR}/deb
	${SCC} -c ${CFLAGS} ${COVFLAGS} -o $@ $<

#
# BUILDDIR
#
${BUILDDIR}/deb: Makefile $(wildcard */Makefile) Makefile.vars
	${SMKDIR} $(BUILDDIR)
	@touch $@

# 
# File dependencies
#
${BUILDDIR}/scan.c: scan.l
${BUILDDIR}/defs.c: defs.y ${BUILDDIR}/scan.c
${BUILDDIR}/defs.o: ${BUILDDIR}/scan.c

include Random/Makefile
include Examples/Makefile
-include $(OBJS:.o=.d)

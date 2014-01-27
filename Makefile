
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

OBJ  = ${addprefix ${BUILDDIR}/, dealer.o defs.o pbn.o c4.o getopt.o pointcount.o}

${PROGRAM}: ${OBJ}
	$(SCC) -o $@ $?

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
	${SFLEX} -t $< >$@

#
# Yacc
#
${BUILDDIR}/%.c: %.y ${BUILDDIR}/deb
	${SYACC} $<
	mv -f y.tab.c $@

#
# C-code
#
${BUILDDIR}/%.o: %.c ${BUILDDIR}/deb
	${SCC} -c ${CFLAGS} -o $@ $<

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

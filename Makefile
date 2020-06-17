# $Header: /home/henk/CVS/dealer/Makefile,v 1.15 1999/08/05 19:57:44 henk Exp $

CC      = gcc
CFLAGS = -Wall -pedantic -I. -DNDEBUG -c
FLEX    = flex
YACC    = yacc
# Note: this should be the Berkeley Yacc, sometimes called byacc

PROGRAM  = dealer
TARFILE  = ${PROGRAM}.tar
GZIPFILE = ${PROGRAM}.tar.gz

SRC  = dealer.c pbn.c  c4.c getopt.c pointcount.c __random.c rand.c srand.c
LSRC = scan.l
YSRC = defs.y
HDR  = dealer.h tree.h

OBJ  = dealer.o defs.o pbn.o c4.o getopt.o pointcount.o __random.o rand.o srand.o
LOBJ = scan.c
YOBJ = defs.c


dealer: ${OBJ} ${LOBJ} ${YOBJ}
	$(CC) -o $@ ${OBJ} 
	
clean:
	rm -f ${OBJ} ${LOBJ} ${YOBJ} 
	${MAKE} -C Examples clean

tarclean: clean ${YOBJ}
	rm -f ${PROGRAM}
	rm -f ${TARFILE}  ${GZIPFILE}

tarfile: tarclean
	cd .. ; \
        rm ${TARFILE} ${GZIPFILE} ; \
	tar cvf ${TARFILE} ${PROGRAM} ; \
	mv ${TARFILE} ${PROGRAM} 
	gzip -f ${TARFILE}

test: dealer
	${MAKE} -C Examples test

#
# Lex
#
.l.c:
	${FLEX} -t $< >$@

#
# Yacc
#
.y.c:
	${YACC} $<
	mv -f y.tab.c $@

#
# C-code
#
.c.o:
	${CC} ${CFLAGS} -o $@ $<

# 
# File dependencies
#
scan.c: scan.l
defs.c: scan.c defs.y
dealer.o: tree.h scan.l dealer.h defs.c scan.c 
pbn.o: tree.h scan.l dealer.h
defs.o:	tree.h
c4.o: c4.c  c4.h
getopt.o: getopt.h
pointcount.o: pointcount.h

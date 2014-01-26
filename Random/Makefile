SRC      = $(wildcard *.c) 
OBJ      = $(subst .c,.o,$(wildcard *.c)) 
GCC      = gcc
GCCFLAGS = -c -Wall -O2 -I.
HDR      = ansidecl.h
LIB      = libgnurand.a
AR       = ar
ARFLAGS  = rcs

help:
	@echo "Makefile tags:"
	@echo
	@echo "lib:   Make ${LIB}"
	@echo "clean: Remove ${OBJ}"
	@echo  

lib: ${LIB}


clean:
	-rm ${OBJ} ${LIB}

.c.o: ${HDR}
	${GCC} ${GCCFLAGS} -o $@ $<

${LIB}: ${OBJ} ${HDR}
	${AR} ${ARFLAGS} $@ ${OBJ}

__random.o: ${HDR}
rand.o:     ${HDR}
srand.o:    ${HDR}

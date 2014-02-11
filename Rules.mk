TARGETS := dealer dealergenlib

dealer_LIBS := libgnurand.a

dealer_SRC := dealer.c \
	c4.c \
	pbn.c \
	pointcount.c \
	defs.y \
	scan.l

dealer_INCDIR := .

dealergenlib_SRC := genlib.c

SUBDIRS := */Rules.mk

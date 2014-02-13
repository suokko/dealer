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

ifneq ($(subst mingw,,$(host)),$(host))
dealer_LIBS += -lws2_32
dealergenlib_LIBS := -lws2_32 libgnurand.a
endif

SUBDIRS := */Rules.mk

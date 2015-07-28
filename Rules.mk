TARGETS := dealer dealergenlib libdealer.a
GENTARGETS := tables

tables_SRC := pregen.c
tables_PARAM := -ir
tables_OUT := tables.c

dealer_LIBS := libdealer.a librand.a libcpudetect.a

dealer_SRC := main.c \
	pbn.c \
	pointcount.c \
	defs.y \
	scan.l

dealer_INCDIR := .

dealergenlib_SRC := genlib.c
dealergenlib_LIBS := librand.a

# libdds support
ifeq ($(subst mingw,,$(host)),$(host))
dealer_LIBS += -ldl
endif
dealer_SRC += dds.cpp

ifneq ($(subst mingw,,$(host)),$(host))
dealer_LIBS += -lws2_32
dealergenlib_LIBS += -lws2_32 librand.a
endif

libdealer.a_SRC := dealer.c \
	c4.c \
	tables.c

libdealer.a_MV_SRC := dealer.c c4.c
libdealer.a_MV_CFG := default sse2 popcnt sse4 avx2

libdealer.a_MV_default := -DMVDEFAULT 

ifneq (gcc,$(COMPILER))
libdealer.a_MV_sse2 := -msse -msse2 -mfpmath=sse
libdealer.a_MV_popcnt := -msse -msse2 -msse3 -mpopcnt -mfpmath=sse
libdealer.a_MV_sse4 := -msse -msse2 -msse3 -mpopcnt -msse4.1 -msse4.2 -mfpmath=sse
libdealer.a_MV_avx2 := -msse -msse2 -msse3 -mpopcnt -msse4.1 -msse4.2 -mavx -mavx2 -mfpmath=sse
else
libdealer.a_MV_sse2 := -msse -msse2 
libdealer.a_MV_popcnt := -msse -msse2 -msse3 -mpopcnt 
libdealer.a_MV_sse4 := -msse -msse2 -msse3 -mpopcnt -msse4.1 -msse4.2 
libdealer.a_MV_avx2 := -msse -msse2 -msse3 -mpopcnt -msse4.1 -msse4.2 -mavx  -mavx2 
endif
SUBDIRS := */Rules.mk

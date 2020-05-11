TARGETS := dealer dealergenlib libdealer.a
GENTARGETS := tables

tables_SRC := pregen.cpp
tables_PARAM := -irn
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

libdealer.a_MV_default := -DMVDEFAULT=default

ifneq (gcc,$(COMPILER))
FPMATH := -mfpmath=sse
endif
libdealer.a_MV_sse2 := -msse -msse2 $(FPMATH) -DMVDEFAULT=sse2
libdealer.a_MV_popcnt := -msse -msse2 -msse3 -mpopcnt $(FPMATH) -DMVDEFAULT=popcnt
libdealer.a_MV_sse4 := -msse -msse2 -msse3 -mpopcnt -msse4.1 -msse4.2 $(FPMATH) -DMVDEFAULT=ss4
libdealer.a_MV_avx2 := -msse -msse2 -msse3 -mpopcnt -msse4.1 -msse4.2 -mavx -mavx2 $(FPMATH) -DMVDEFAULT=avx2

# Rule to make sure git submodule has been fetched

dds/README.md:
	$(SGIT) submodule init
	$(SGIT) submodule update

dds.cpp_DEP := dds/README.md

SUBDIRS := */Rules.mk

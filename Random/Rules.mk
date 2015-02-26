TARGETS := librand.a

librand.a_SRC := SFMT.c

librand.a_MV_CFG := default sse2 avx2
librand.a_MV_SRC := SFMT.c

librand.a_MV_default := -DMVSFMT=default
ifneq (gcc,$(COMPILER))
FPMATH := -mfpmath=sse
endif
librand.a_MV_sse2 := -msse -msse2 $(FPMATH) -DMVSFMT=sse2
librand.a_MV_avx2 := -msse -msse2 -msse3 -mpopcnt -msse4.1 -msse4.2 -mavx -mavx2 $(FPMATH) -DMVSFMT=avx2

librand.a_CFLAGS := -DSFMT_MEXP=19937

TARGETS := librand.a

librand.a_SRC := SFMT.c

librand.a_MV_CFG := default sse2 avx2
librand.a_MV_SRC := SFMT.c

librand.a_MV_default := -DMVDEFAULT
ifneq ($(subst gcc,,$(COMPILERVERSION)),$(COMPILERVERSION))
librand.a_MV_sse2 := -msse -msse2 -mfpmath=sse
librand.a_MV_avx2 := -msse -msse2 -msse3 -mpopcnt -msse4.1 -msse4.2 -mavx -mavx2 -mfpmath=sse
else
librand.a_MV_sse2 := -msse -msse2
librand.a_MV_avx2 := -msse -msse2 -msse3 -mpopcnt -msse4.1 -msse4.2 -mavx -mavx2
endif

librand.a_CFLAGS := -DSFMT_MEXP=19937

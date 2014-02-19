TARGETS := librand.a

librand.a_SRC := SFMT.c

librand.a_MV_CFG := default sse2

librand.a_MV_default :=
librand.a_MV_sse2 := -msse2

librand.a_CFLAGS := -DSFMT_MEXP=19937

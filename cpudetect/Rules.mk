TARGETS := cpudetect libcpudetect.a

libcpudetect.a_SRC := detect.cpp

cpudetect_SRC := main.cpp target.cpp

cpudetect_LIBS := libcpudetect.a

# FIXME Stupid gcc doesn't know to unroll popcount loop
# even tough that would eliminate slow integer division
# clang managed to unroll the loop with only -O2 flag set
cpudetect_CXXFLAGS := -funroll-loops

cpudetect_MV_SRC := target.cpp

cpudetect_MV_CFG := default slowmul sse2 sse3 popcnt sse4 avx avx2

cpudetect_MV_default := -DMVDEFAULT 
cpudetect_MV_slowmul := -DSLOWMUL=1
ifneq (gcc,$(COMPILER))
cpudetect_MV_sse2 := -msse -msse2 -mfpmath=sse
cpudetect_MV_sse3 := -msse -msse2 -msse3 -mfpmath=sse
cpudetect_MV_popcnt := -msse -msse2 -msse3 -mpopcnt -mfpmath=sse
cpudetect_MV_sse4 := -msse -msse2 -msse3 -mpopcnt -msse4.1 -msse4.2 -mfpmath=sse
cpudetect_MV_avx := -msse -msse2 -msse3 -mpopcnt -msse4.1 -msse4.2 -mavx -mfpmath=sse
cpudetect_MV_avx2 := -msse -msse2 -msse3 -mpopcnt -msse4.1 -msse4.2 -mavx  -mavx2 -mfpmath=sse
else
cpudetect_MV_sse2 := -msse -msse2 
cpudetect_MV_sse3 := -msse -msse2 -msse3 
cpudetect_MV_popcnt := -msse -msse2 -msse3 -mpopcnt 
cpudetect_MV_sse4 := -msse -msse2 -msse3 -mpopcnt -msse4.1 -msse4.2 
cpudetect_MV_avx := -msse -msse2 -msse3 -mpopcnt -msse4.1 -msse4.2 -mavx 
cpudetect_MV_avx2 := -msse -msse2 -msse3 -mpopcnt -msse4.1 -msse4.2 -mavx  -mavx2 
endif

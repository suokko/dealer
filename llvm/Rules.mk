LLVMCONFIG = llvm-config
LLVMCFLAGS   = ${shell ${LLVMCONFIG} --cxxflags}
ifeq (0,1)
LLVMLIBS     = ${shell ${LLVMCONFIG}  --system-libs --libs core native jit interpreter} 
LLVMLDFLAGS  = ${shell ${LLVMCONFIG} --ldflags}
else
LLVMLIBS     = -lLLVM-3.5
LLVMLDFLAGS  =
endif

TARGETS := libdealerllvm.a testllvm

libdealerllvm.a_SRC := *.cpp

testllvm_SRC := main.cpp
testllvm_LIBS := libdealerllvm.a $(LLVMLIBS)
testllvm_CPPFLAGS := $(LLVMCFLAGS)
testllvm_LDFLAGS := $(LLVMLDFLAGS)

libdealerllvm.a_CPPFLAGS := $(LLVMCFLAGS)

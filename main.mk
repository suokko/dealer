
V	 ?= 0

FLEX	 ?= flex
YACC	 ?= yacc
RM	 ?= rm -f
RMDIR	 ?= rmdir
ARFLAGS  := rcs
MKDIR    ?= mkdir -p
INSTALL	 ?= install
POD2MAN  ?= pod2man

CFLAGS   ?=
CPPFLAGS ?= -march=native  -flto
CXXFLAGS ?=
LDFLAGS  ?=
DESTDIR  ?=

COMPILERVERSION := $(shell $(CC) --version)

ifneq ($(subst Free Software Foundation,,$(COMPILERVERSION)),$(COMPILERVERSION))
AR := $(host)gcc-ar
COMPILER:=gcc
else
ifneq ($(subst clang,,$(COMPILERVERSION)),$(COMPILERVERSION))
AR := $(host)llvm-ar
COMPILER:=clang
else
$(warning *** Warning: unknown compiler)
COMPILER:=unknown
endif
endif

RELEASEFLAGS := -DNDEBUG
ifeq (gcc,$(COMPILER))
OPTFLAGS := -O2 -ftree-vectorize -fvect-cost-model
else
OPTFLAGS := -O2 -ftree-vectorize
endif
PROFFLAGS := -fprofile-generate
ifeq ($(filter release,$(MAKECMDGOALS)),release)
PUSEFLAGS := -fprofile-use -flto=jobserver
OPTFLAGS += $(RELEASEFLAGS)
LDFLAGS += -fwhole-program
else
PUSEFLAGS :=
endif

#Disable inlining for coverage reports
COVFLAGS := -fno-inline-small-functions -fno-indirect-inlining -fno-partial-inlining --coverage
DCFLAGS   := -std=gnu11 $(CFLAGS)
DCPPFLAGS := -MP -MD -Wall -Wextra -g $(CPPFLAGS) 
DCXXFLAGS := -std=c++11 $(CXXFLAGS)
DLDFLAGS  := $(LDFLAGS)

prefix	?=/usr/local
bindir	?=$(prefix)/bin
datarootdir ?=$(prefix)/share
docdir  ?=$(datarootdir)/doc/dealer
mandir  ?=$(datarootdir)/man
build	?=
host	?= $(build)

BUILDDIR = .libs
PROGRAM  = dealer
PROGRAMCOV = ${PROGRAM}.cov

ifeq ($V, 1)
SCC    = $(host)$(CC)
SAR    = $(host)$(AR)
SCXX   = $(host)$(CXX)
SFLEX  = $(FLEX)
SYACC  = $(YACC)
SMKDIR = $(MKDIR)
SRM    = $(RM)
SRMDIR = $(RMDIR)
SINSTALL = $(INSTALL)
SPOD2MAN = $(POD2MAN)
SGEN   = 
else
SCC    = @echo "  CC    " ${notdir $@} && $(host)$(CC)
SAR    = @echo "  AR    " ${notdir $@} && $(host)$(AR)
SCXX   = @echo "  CXX   " ${notdir $@} && $(host)$(CXX)
SFLEX  = @echo "  FLEX  " ${notdir $@} && $(FLEX)
SYACC  = @echo "  YACC  " ${notdir $@} && $(YACC)
SMKDIR = @echo "  MKDIR " ${notdir $@} && $(MKDIR)
SRM    = @echo "  RM    " $@ && $(RM)
SRMDIR = @echo "  RMDIR " $@ && $(RMDIR)
SINSTALL = @echo "  INST  " ${notdir $@} && $(INSTALL)
SPOD2MAN = @echo "  P2M   " ${notdir $@} && $(POD2MAN)
SGEN   = @echo "  GEN   " $$@ && 
endif


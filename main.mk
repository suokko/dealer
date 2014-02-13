
V	 ?= 0

FLEX	 ?= flex
YACC	 ?= yacc
RM	 ?= rm -f
RMDIR	 ?= rmdir
ARFLAGS  := rcs
MKDIR    ?= mkdir -p
INSTALL	 ?= install

CFLAGS   ?= -march=native 
CXXFLAGS ?=
LDFLAGS  ?=
DESTDIR  ?=


RELEASEFLAGS := -DNDEBUG
OPTFLAGS := -O2 -ftree-vectorize -fvect-cost-model $(CFLAGS)
PROFFLAGS := -fprofile-generate
PUSEFLAGS := -fprofile-use -flto
ifeq ($(filter release,$(MAKECMDGOALS)),release)
OPTFLAGS += $(RELEASEFLAGS)
endif

#Disable inlining for coverage reports
COVFLAGS := -fno-inline-small-functions -fno-indirect-inlining -fno-partial-inlining --coverage
DCFLAGS   := -MP -MD -Wall -pedantic -g
DCXXFLAGS := -std=c++11 $(CXXFLAGS)
DLDFLAGS  := $(LDFLAGS)

prefix	?=/usr/local
bindir	?=$(prefix)/bin
datarootdir ?=$(prefix)/share
docdir  ?=$(datarootdir)/doc/dealer
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
endif


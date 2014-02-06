
V	 ?= 0

FLEX	 ?= flex
YACC	 ?= yacc
RM	 ?= rm -f
RMDIR	 ?= rmdir
ARFLAGS  := rcs
MKDIR    := mkdir -p

EXTRACFLAGS ?= -march=native 
DESTDIR     ?=

RELEASEFLAGS := -DNDEBUG
OPTFLAGS := -O2 -ftree-vectorize -fvect-cost-model $(EXTRACFLAGS)
PROFFLAGS := -fprofile-generate
PUSEFLAGS := -fprofile-use -flto
ifeq ($(filter release,$(MAKECMDGOALS)),release)
OPTFLAGS += $(RELEASEFLAGS)
endif

#Disable inlining for coverage reports
COVFLAGS := -fno-inline-small-functions -fno-indirect-inlining -fno-partial-inlining --coverage
CFLAGS   := -MP -MD -Wall -pedantic -g
CXXFLAGS := -std=c++11

PREFIX   ?=/usr/local
BINPREFIX ?=$(PREFIX)/bin

BUILDDIR = .libs
PROGRAM  = dealer
PROGRAMCOV = ${PROGRAM}.cov
GENLIB = dealerlibrarygenerator

ifeq ($V, 1)
SCC    = $(CC)
SAR    = $(AR)
SCXX   = $(CXX)
SFLEX  = $(FLEX)
SYACC  = $(YACC)
SMKDIR = $(MKDIR)
SRM    = $(RM)
SRMDIR = $(RMDIR)
else
SCC    = @echo "  CC    " ${notdir $@} && $(CC)
SAR    = @echo "  AR    " ${notdir $@} && $(AR)
SCXX   = @echo "  CXX   " ${notdir $@} && $(CXX)
SFLEX  = @echo "  FLEX  " ${notdir $@} && $(FLEX)
SYACC  = @echo "  YACC  " ${notdir $@} && $(YACC)
SMKDIR = @echo "  MKDIR " ${notdir $@} && $(MKDIR)
SRM    = @echo "  RM    " $@ && $(RM)
SRMDIR = @echo "  RMDIR " $@ && $(RMDIR)
endif


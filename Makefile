# Some helpers

# Make the path to be relative to working directory or
# absolute if it is below current working directory
define CANONICAL_PATH
$(subst $(CURDIR),.,$(patsubst $(CURDIR)/%,%,$(abspath $(1))))
endef

# Find main.mk in the directory tree
# This supports make on any subdirectory with symlinked Makefile
ifndef TOP
TOP := $(shell \
	totest=$(CURDIR); \
	while [ ! -r "$$totest/main.mk" ] && [ "$$totest" != "" ]; do \
		totest=$${totest%/*};  \
	done; \
	echo $$totest)
endif

# All libraries should go to the top level build directory
LIB_DIR = $(call CANONICAL_PATH,${TOP}/${BUILDDIR})
# The target binary is excepted to appear in the top build directory
TARGET_DIR := $(call CANONICAL_PATH,${TOP})
ALL_TARGETS :=
# Helper variables to handle silent compilation
LINK.c = $(SCC) $(CFLAGS) -o $@ $^
LINK.cxx = $(SCXX) $(CFLAGS) $(CXXFLAGS) -o $@ $^
COMPILE.c = $(SCC) -c $(CFLAGS) -o $@ $<
COMPILE.cxx = $(SCXX) -c $(CFLAGS) $(CXXFLAGS) -o $@ $<
STATICLIB = $(SAR) ${ARFLAGS} $@ $?

# Creates all make rules required to build and install a target
# It should also include rules to install other depending files. 
define CREAT_TARGET_RULE
ifeq ($$(${1}_TYPE),prog)
$$(${1}_PATH): $${${1}_OBJS}
	@mkdir -p $(dir $$@)
	$$(LINK.c)

install: install_${1}

install_${1}: $$(${1}_PATH)
	@[ -z "$(DESTDIR)" ] || mkdir -p $(DESTDIR)$(BINPREFIX)
	/usr/bin/install -m 0755 $$(${1}_PATH) $(DESTDIR)$(BINPREFIX)/$$(notdir $$(${1}_PATH))

uninstall: uninstall_${1}

uninstall_${1}:
	$${SRM} $(DESTDIR)$(BINPREFIX)/$$(notdir $$(${1}_PATH))

$$(${1}_COVPATH): $${${1}_COVOBJS}
	@mkdir -p $(dir $$@)
	$$(LINK.c)

$$(${1}_PROFPATH): $${${1}_PROFOBJS}
	@mkdir -p $(dir $$@)
	$$(LINK.c)
else
ifeq "$(${1}_TYPE)" "static"
$$(${1}_PATH): $${${1}_OBJS}
	@mkdir -p $(dir $$@)
	$$(STATICLIB)

$$(${1}_COVPATH): $${${1}_COVOBJS}
	@mkdir -p $(dir $$@)
	$$(STATICLIB)

$$(${1}_PROFPATH): $${${1}_PROFOBJS}
	@mkdir -p $(dir $$@)
	$$(STATICLIB)
else
	$$(warning "Unknown target type: ${1} -> $(${1}_TYPE)" )
endif
endif
endef

# Generate clean rule for a target
define CREAT_CLEAN_RULE
ifneq ($(abspath $(${1}_BUILDDIR)),$(${1}_BUILDDIR))
clean: clean_${1}
endif

clean_${1}:
	$$(SRM) $$(${1}_CLEAN)
	-$$(SRMDIR) $$(${1}_BUILDDIR)
endef

# Setup dependencies for each target so all sources are build into the target
define MAKE_TARGET
# Basic path and target type variable setup
ifneq "$${DIR}" ""
${1}_BUILDDIR := $$(call CANONICAL_PATH,$${DIR}/${BUILDDIR})
INCFLAGS := $$(addprefix -I,$$(addprefix $${DIR}/,$$(${1}_INCDIR)))
else
${1}_BUILDDIR := $$(call CANONICAL_PATH,${BUILDDIR})
INCFLAGS := $$(addprefix -I,$$(${1}_INCDIR))
endif
ifeq "$(filter %.a,${1})" ""
TDIR := $${TARGET_DIR}
${1}_TYPE=prog
else
TDIR := $${LIB_DIR}
${1}_TYPE=static
endif
ifneq "$${TDIR}" ""
# Figure correct path for the target
${1}_PATH := $$(call CANONICAL_PATH,$$(addprefix $${TDIR}/,${1}))
else
${1}_PATH := $$(call CANONICAL_PATH,${1})
endif
# fugure out what should be build from the source file extension
${1}_OBJS := $$(call CANONICAL_PATH,$$(addprefix $$(${1}_BUILDDIR)/,\
	$$(filter-out %.l,$$(subst .cpp,.o,$$(subst .y,.o,\
	$$(subst .c,.o,$${${1}_SRC}))))))
${1}_FLEXS := $$(call CANONICAL_PATH,$$(addprefix $$(${1}_BUILDDIR)/,\
	$$(subst .l,.c,$$(filter %.l,$${${1}_SRC}))))
${1}_YACCS := $$(call CANONICAL_PATH,$$(addprefix $$(${1}_BUILDDIR)/,\
	$$(subst .y,.o,$$(filter %.y,$${${1}_SRC}))))

# Setup shadow objects for coverage and profile targets with different flags
${1}_COVOBJS := $$(subst .o,.cov.o,$$(${1}_OBJS))
${1}_PROFOBJS := $$(subst .o,.prof.o,$$(${1}_OBJS))
${1}_PROFYACCS := $$(subst .o,.prof.o,$$(${1}_YACCS))
${1}_PROFFLEXS := $$(subst .c,.prof.c,$$(${1}_FLEXS))

# Check if we need CXX compiler
ifeq "$$(filter $(CXX_EXTS),$$(${1}_SRC))" ""
${1}_HASCXX := 1
else
${1}_HASCXX := 0
endif

ALL_TARGETS += ${1}

# Coverage and profile targets with standard naming
ifeq ($$(${1}_TYPE),prog)
${1}_COVPATH := $$(addsuffix .cov,$$(${1}_PATH))
${1}_PROFPATH := $$(addsuffix .prof,$$(${1}_PATH))
else
${1}_COVPATH := $$(subst .a,.cov.a,$$(${1}_PATH))
${1}_PROFPATH := $$(subst .a,.prof.a,$$(${1}_PATH))
endif

#Main target dependencies only if it is current directory or subdirectory
ifeq ($$(filter-out $(CURDIR),$$(abspath $$(DIR))),)
all: $$(${1}_PATH)
prof: $$(${1}_PROFPATH)
cov: $$(${1}_COVPATH)
endif

#Target variables
ifneq ($(filter release,$(MAKECMDGOALS)),)
$$(${1}_OBJS): CFLAGS := $(CFLAGS) $$(${1}_CFLAGS) $(OPTFLAGS) $(PUSEFLAGS) $$(INCFLAGS)
else
$$(${1}_OBJS): CFLAGS := $(CFLAGS) $$(${1}_CFLAGS) $(OPTFLAGS) $$(INCFLAGS)
endif
$$(${1}_OBJS): CXXFLAGS := $(CXXFLAGS) $$(${1}_CXXFLAGS)
$$(${1}_COVOBJS): CFLAGS := $(CFLAGS) $$(${1}_CFLAGS) $(OPTFLAGS) $(COVFLAGS) $$(INCFLAGS)
$$(${1}_COVOBJS): CXXFLAGS := $(CXXFLAGS) $$(${1}_CXXFLAGS)
$$(${1}_PROFOBJS): CFLAGS := $(CFLAGS) $$(${1}_CFLAGS) $(OPTFLAGS) $(PROFFLAGS) $$(INCFLAGS)
$$(${1}_PROFOBJS): CXXFLAGS := $(CXXFLAGS) $$(${1}_CXXFLAGS)
ifneq ($(filter release,$(MAKECMDGOALS)),)
$${${1}_PATH}: CFLAGS := $(CFLAGS) $$(${1}_CFLAGS) $(OPTFLAGS) $(PUSEFLAGS)
else
$${${1}_PATH}: CFLAGS := $(CFLAGS) $$(${1}_CFLAGS) $(OPTFLAGS)
endif
$$(${1}_PATH): CXXFLAGS := $(CXXFLAGS) $$(${1}_CXXFLAGS)
$${${1}_PROFPATH}: CFLAGS := $(CFLAGS) $$(${1}_CFLAGS) $(OPTFLAGS) $(PROFFLAGS)
$$(${1}_PROFPATH): CXXFLAGS := $(CXXFLAGS) $$(${1}_CXXFLAGS)
$${${1}_COVPATH}: CFLAGS := $(CFLAGS) $$(${1}_CFLAGS) $(OPTFLAGS) $(COVFLAGS)
$$(${1}_COVPATH): CXXFLAGS := $(CXXFLAGS) $$(${1}_CXXFLAGS)

# Libary dependencies
# TODO check for c++ static libraries
# TODO support shared libraries
$${${1}_PATH}: $$(addprefix $$(LIB_DIR)/,$$(${1}_LIBS))
$${${1}_COVPATH}: $$(addprefix $$(LIB_DIR)/,$$(subst .a,.cov.a,$$(${1}_LIBS)))
$${${1}_PROFPATH}: $$(addprefix $$(LIB_DIR)/,$$(subst .a,.prof.a,$$(${1}_LIBS)))

# Setup flex and yacc dependencies
ifneq "$$(${1}_YACCS)" ""
$$(${1}_YACCS): $$(${1}_FLEXS) $$(subst .o,.c,$$(${1}_YACCS))
ifneq ($(filter release,$(MAKECMDGOALS)),)
$$(${1}_YACCS): CFLAGS := $(CFLAGS) $$(${1}_CFLAGS) -Wno-unused-function $(PUSEFLAGS) $$(INCFLAGS)
else
$$(${1}_YACCS): CFLAGS := $(CFLAGS) $$(${1}_CFLAGS) -Wno-unused-function  $$(INCFLAGS)
endif
$$(subst .o,.cov.o,$$(${1}_YACCS)): $$(${1}_FLEXS) $$(subst .o,.c,$$(${1}_YACCS))
$$(subst .o,.cov.o,$$(${1}_YACCS)): CFLAGS := $(CFLAGS) $$(${1}_CFLAGS) -Wno-unused-function $(COVFLAGS) $$(INCFLAGS)
$$(${1}_PROFYACCS): $$(${1}_PROFFLEXS) $$(subst .o,.c,$$(${1}_PROFYACCS))
$$(${1}_PROFYACCS): CFLAGS := $(CFLAGS) $$(${1}_CFLAGS) -Wno-unused-function  $(PROFFLAGS) $$(INCFLAGS)
endif

ALLOBJS := $$(${1}_OBJS) $$(${1}_COVOBJS) $${${1}_PROFOBJS}
GENFILES := $$(subst .o,.c,$$(${1}_YACCS)) $$(subst .o,.c,$$(${1}_PROFYACCS)) $$(${1}_PROFFLEXS) $$(${1}_FLEXS)

# All objets should depend on makefile changes that affect their rules
$$(ALLOBJS) $$(GENFILES): $$(call CANONICAL_PATH,$$(MKFILE) $(TOP)/main.mk $(TOP)/Makefile)

# Include all automatically generated dependencies
-include $$(subst .o,.d,$$(ALLOBJS))

# listt all created files for clean target.
${1}_CLEAN := $$(${1}_PATH) $$(${1}_COVPATH) $$(${1}_PROFPATH) $$(GENFILES) $$(ALLOBJS) $$(subst .o,.d,$$(ALLOBJS)) $$(subst .o,.gcda,$$(ALLOBJS)) $$(subst .o,.gcno,$$(ALLOBJS))

endef

# Include each listed subdirectory
define INCLUDE_SUBDIR
# Clean and setup per subdirectory variables
# TODO support not target prefixed variables for single target subdirectoreis
TARGETS :=
SUBDIRS :=
MKFILE  := $(1)

# Keep track of current subdiretory processing
DIR := $(call CANONICAL_PATH,$(dir $(1)))
DIR_STACK := $$(call PUSH,$$(DIR_STACK),$$(DIR))

# Time to process the makefile
include $(1)

# Generated rules for each target declared
$$(foreach TARGET, $${TARGETS},\
$$(eval $$(call MAKE_TARGET,$${TARGET}))\
)

# Include subdiretories of subdiretories
ifneq "$$(DIR)" ""
$$(foreach SUB, $$(SUBDIRS),\
$$(eval $$(call INCLUDE_SUBDIR,$$(addprefix $$(DIR)/,$${SUB}))))
else
$$(foreach SUB, $$(SUBDIRS),\
$$(eval $$(call INCLUDE_SUBDIR,$${SUB})))
endif

# Time to pop back the diretory state for the parent directory
DIR_STACK := $$(call POP,$$(DIR_STACK))
DIR := $$(call PEEK,$$(DIR_STACK))
endef

define MIN
$(firstword $(sort $(1) $(2)))
endef

# Feth the top of stack
define PEEK
$(lastword $(subst :, ,$(1)))
endef

# Remove the top of stack
define POP
$(1:%:$(lastword $(subst :, ,$(1)))=%)
endef

# Add entry to the top of stack
define PUSH
$(2:%=$(1):%)
endef

# add dependency between profile results and release build objects
# to allow release build to use profile guided optimization
define PROFILE_DEPEND
$$(${1}_OBJS): $$(subst .o,.gcda,$$(${1}_OBJS))
$$(subst .o,.gcda,$$(${1}_OBJS)): profiletest
endef

# Clean old profile output before profiling the current build
define PROFOBJS_DEPEND_CLEAN
$$(${1}_PROFOBJS): clean_gcda
$$(${1}_PROFFLEXS): clean_gcda
$$(subst .o,.c,$$(${1}_PROFYACCS)): clean_gcda

clean_gcda: clean_${1}_gcda

clean_${1}_gcda:
	$${SRM} $$(${1}_BUILDDIR)/*.gcda $$(${1}_BUILDDIR)/*.gcno
endef

# Basic build
all:
	@echo Target $@ done!

# Profile guide optimization build
release: all
	@echo Target $@ done!

# Build to generate coverage data for unit tests
cov:
	@echo Target $@ done!

# Build to generate profiling data for optimization
prof:
	@echo Target $@ done!

# Helper to create rules between sources and resutls. It generates rules for 
# auto generated sources also.
define CREAT_OBJECT_RULE
${1}/${2}: $(call CANONICAL_PATH,${1}/../${3})
	${4}

${1}/${2}: ${1}/${3}
	${4}
endef

# Generate actual rule body for C sources
define C_RULE
	@mkdir -p $$(dir $$@)
	$${COMPILE.c}
endef

# Generate actual rule body for C++ sources
define CXX_RULE
	@mkdir -p $$(dir $$@)
	$${COMPILE.c}
endef

# Generate actual rule body for yacc sources
define YACC_RULE
	@mkdir -p $$(dir $$@)
	$$(SYACC) $$< -o $$@
endef

# Generate actual rule body for yacc profile sources
define PROFYACC_RULE
	@mkdir -p $$(dir $$@)
	$$(SYACC) $$< -o $$@
	@sed -i 's/scan.c/scan.prof.c/' $$@
endef

define FLEX_RULE
	@mkdir -p $$(dir $$@)
	$$(SFLEX) -o $$@ $$<
endef

# Declare all supported extensions for different file types
C_EXTS := %.c
FLEX_EXTS := %.l
YACC_EXTS := %.y
CXX_EXTS := %.cpp %.cxx

$(eval $(call INCLUDE_SUBDIR,$(call CANONICAL_PATH,$(TOP)/main.mk)))
$(eval $(call INCLUDE_SUBDIR,$(call CANONICAL_PATH,$(TOP)/Rules.mk)))

# Rename profile results to be used by object build
profiletest: ${ALLTESTS}
	@find -name "*.gcda" | sed 's/^\(.*\).prof\(.*\)$$/mv \0 \1\2/' | $(SHELL)

# Generate rules to clean *.gcda and *.gcno files
$(foreach TARGET,${ALL_TARGETS},\
	$(eval $(call PROFOBJS_DEPEND_CLEAN,${TARGET})))

# Add profiling dependency to the release build
ifneq ($(filter release,$(MAKECMDGOALS)),)
$(foreach TARGET,${ALL_TARGETS},\
	$(eval $(call PROFILE_DEPEND,${TARGET})))
endif

# Create rules for all targets
$(foreach TARGET,${ALL_TARGETS},\
	$(eval $(call CREAT_TARGET_RULE,${TARGET})))

# Create rules for each file type in all targets
$(foreach TARGET,${ALL_TARGETS},\
	$(foreach EXT,$(C_EXTS),\
 	$(eval $(call CREAT_OBJECT_RULE,$(${TARGET}_BUILDDIR),%.o,${EXT},${C_RULE}))))

$(foreach TARGET,${ALL_TARGETS},\
	$(foreach EXT,$(C_EXTS),\
 	$(eval $(call CREAT_OBJECT_RULE,$(${TARGET}_BUILDDIR),%.cov.o,${EXT},${C_RULE}))))

$(foreach TARGET,${ALL_TARGETS},\
	$(foreach EXT,$(C_EXTS),\
 	$(eval $(call CREAT_OBJECT_RULE,$(${TARGET}_BUILDDIR),%.prof.o,${EXT},${C_RULE}))))

$(foreach TARGET,${ALL_TARGETS},\
	$(foreach EXT,$(CXX_EXTS),\
 	$(eval $(call CREAT_OBJECT_RULE,$(${TARGET}_BUILDDIR),%.o,${EXT},${CXX_RULE}))))

$(foreach TARGET,${ALL_TARGETS},\
	$(foreach EXT,$(CXX_EXTS),\
 	$(eval $(call CREAT_OBJECT_RULE,$(${TARGET}_BUILDDIR),%.cov.o,${EXT},${CXX_RULE}))))

$(foreach TARGET,${ALL_TARGETS},\
	$(foreach EXT,$(CXX_EXTS),\
 	$(eval $(call CREAT_OBJECT_RULE,$(${TARGET}_BUILDDIR),%.prof.o,${EXT},${CXX_RULE}))))

$(foreach TARGET,${ALL_TARGETS},\
	$(foreach EXT,$(FLEX_EXTS),\
 	$(eval $(call CREAT_OBJECT_RULE,$(${TARGET}_BUILDDIR),%.c,${EXT},${FLEX_RULE}))))

$(foreach TARGET,${ALL_TARGETS},\
	$(foreach EXT,$(YACC_EXTS),\
 	$(eval $(call CREAT_OBJECT_RULE,$(${TARGET}_BUILDDIR),%.c,${EXT},${YACC_RULE}))))

$(foreach TARGET,${ALL_TARGETS},\
	$(foreach EXT,$(FLEX_EXTS),\
 	$(eval $(call CREAT_OBJECT_RULE,$(${TARGET}_BUILDDIR),%.prof.c,${EXT},${FLEX_RULE}))))

$(foreach TARGET,${ALL_TARGETS},\
	$(foreach EXT,$(YACC_EXTS),\
 	$(eval $(call CREAT_OBJECT_RULE,$(${TARGET}_BUILDDIR),%.prof.c,${EXT},${PROFYACC_RULE}))))

# Generate clean rules for all targets.
$(foreach TARGET,${ALL_TARGETS},\
	$(eval $(call CREAT_CLEAN_RULE,${TARGET})))


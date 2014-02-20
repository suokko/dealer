# Some helpers

# Make the path to be relative to working directory or
# absolute if it is below current working directory
define CANONICAL_PATH
$(subst $(CURDIR),.,$(patsubst $(CURDIR)/%,%,$(abspath $(1))))
endef

# Concat a path to list of files
define CONCAT
$(if ${1},$(call CANONICAL_PATH,$(addprefix ${1}/,${2})),$(call CANONICAL_PATH,${2}))
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
LIB_DIR = $(call CONCAT,${TOP},${BUILDDIR})
# The target binary is excepted to appear in the top build directory
TARGET_DIR := $(call CANONICAL_PATH,${TOP})
ALL_TARGETS :=
ALL_BUILDDIRS :=
# Helper variables to handle silent compilation
LINK.c = +$(SCC) $(DCPPFLAGS) $(DCFLAGS) $(DLDFLAGS) -o $@ $^
LINK.cxx = +$(SCXX) $(DCPPFLAGS) $(DCXXFLAGS) $(DLDFLAGS) -o $@ $^
COMPILE.c = $(SCC) -c $(DCPPFLAGS) $(DCFLAGS) -o $@ $<
COMPILE.cxx = $(SCXX) -c $(DCPPFLAGS) $(DCXXFLAGS) -o $@ $<
STATICLIB = $(SAR) ${ARFLAGS} $@ $?

# Creates all make rules required to build and install a target
# It should also include rules to install other depending files. 
define CREAT_TARGET_RULE
ifeq ($$(${1}_TYPE),prog)

$$(${1}_PATH): $${${1}_OBJS}
	@mkdir -p $(dir $$@)
	$$(LINK.$$(${1}_HASCXX)) $$(${1}_SYSLIBS)

install: install_${1}

install_${1}: $(DESTDIR)$(bindir)/$$(notdir $$(${1}_PATH))

$(DESTDIR)$(bindir)/$$(notdir $$(${1}_PATH): )$$(${1}_PATH)
	@[ -z "$(DESTDIR)" ] || mkdir -p $(DESTDIR)$(bindir)
	$$(SINSTALL) -m 0755 $$(${1}_PATH) $(DESTDIR)$(bindir)/$$(notdir $$(${1}_PATH))

uninstall: uninstall_${1}

uninstall_${1}:
	$${SRM} $(DESTDIR)$(bindir)/$$(notdir $$(${1}_PATH))

$$(${1}_COVPATH): $${${1}_COVOBJS}
	@mkdir -p $(dir $$@)
	$$(LINK.$$(${1}_HASCXX)) $$(${1}_SYSLIBS)

$$(${1}_PROFPATH): $${${1}_PROFOBJS}
	@mkdir -p $(dir $$@)
	$$(LINK.$$(${1}_HASCXX)) $$(${1}_SYSLIBS)
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

# 1: list of files
# 2: submake directory
# 3: extra directory to append to install path
# 4: install path
# 5: type
# 6: permissions
define MAKE_INST
ifneq (,${1})
DST_${5}_${2} := $(if ${3},$$(call CONCAT,$(DESTDIR)${4},${3}),$(DESTDIR)${4})
IDOCS_${5}_${2} := $$(call CONCAT,$$(DST_${5}_${2}),$(if ${2},$(subst ${2}/,,${1}),${1}))
$$(IDOCS_${5}_${2}): $$(call CONCAT,$$(DST_${5}_${2}),%): $(call CONCAT,${2},%)
	@[ -z "$(DESTDIR)" ] || mkdir -p $$(DST_${5}_${2})
	$${SINSTALL} -m ${6} $$< $$@

install: $$(IDOCS_${5}_${2})

uninstall_${5}_${2}:
	$${SRM} $$(IDOCS_${5}_${2})

uninstall: uninstall_${5}_${2}

endif
endef

define MAKE_MAN_INSTALL
$(call MAKE_INST,${1},${2},${3},$(mandir),man,0644)
endef

define MAKE_DOC_INSTALL
$(call MAKE_INST,${1},${2},${3},$(docdir),doc,0644)
endef

define MAKE_BIN_INSTALL
$(call MAKE_INST,${1},${2},${3},$(bindir),bin,0755)
endef

define MAKE_PERLLIB_INSTALL
$(call MAKE_INST,${1},${2},${3},$(call CONCAT,$(datarootdir),perl5),perl,0644)
endef

# Generate clean rule for a target
define CREAT_CLEAN_RULE

clean_dir_$$(${1}_BUILDDIR): clean_${1}
ifeq "$(${1}_TYPE)" "static"
clean_dir_$(LIB_DIR): clean_${1}
endif

clean_${1}:
	$$(SRM) $$(${1}_CLEAN)
endef

define CREAT_CLEAN_DIR_RULE
ifneq ($(abspath ${1}),${1})
clean: clean_dir_${1}
endif

clean_dir_${1}:
	$$(SRMDIR) ${1}

endef

define MAKE_VERSIONED
#Remove dependecy tracking
CPPFLAG := $$(filter-out -M%,$$(DCPPFLAGS))
#If version doesn't define any -m options it is assumed to use compiler
#defaults with some define set to differentiate the code
ifeq ($$(subst -m,,$$(${1}_MV_${2})),$$(${1}_MV_${2}))
CPPFLAG := $$(CPPFLAG)
FLAGSWORK :=
else
CPPFLAG := $$(filter-out -m%,$$(CPPFLAG)) $$(filter -m32,$$(CPPFLAG)) $$(filter -m64,$$(CPPFLAG))
# Cache flag tests result for quicker recompiles
CACHE := $$(LIB_DIR)/__flagtest.o.${1}_${2}.cache
# Check -m flags passed in are accepted by compiler
FLAGSWORK := $$(shell [ -e $$(CACHE) ] || ($(MKDIR) $$(LIB_DIR) && $(CC) -E $$(OPTFLAGS) $$(CPPFLAG) $$(DCFLAGS) $$(${1}_CFLAGS) $$(${1}_MV_${2}) $$(call CONCAT,$(TOP),flagstest.c) -o /dev/null 2> $$(CACHE)); cat $$(CACHE))

FLAGS_CLEAN += $$(CACHE)
endif

MV_OBJS := $$(call CONCAT,$$(${1}_BUILDDIR),\
	$$(patsubst %.cpp,%_${2}.o,\
	$$(patsubst %.c,%_${2}.o,$${${1}_MV_SRC})))

ifeq ($$(FLAGSWORK),)

ifneq ($(filter release,$(MAKECMDGOALS)),)
$$(MV_OBJS):  DCPPFLAGS := $$(OPTFLAGS) $$(CPPFLAG) $$(${1}_CPPFLAGS) $$(PUSEFLAGS) $$(${1}_MV_${2}) $$(INCPPFLAGS)
else
$$(MV_OBJS):  DCPPFLAGS := $$(OPTFLAGS) $$(CPPFLAG) $$(${1}_CPPFLAGS) $$(${1}_MV_${2}) $$(INCPPFLAGS)
endif
$$(MV_OBJS):  DCXXFLAGS := $$(DCXXFLAGS) $$(${1}_CXXFLAGS)
$$(MV_OBJS):  DCFLAGS := $$(DCFLAGS) $$(${1}_CFLAGS)
$$(patsubst %.o,%.cov.o,$$(MV_OBJS)):  DCPPFLAGS := $$(CPPFLAG) $$(${1}_CPPFLAGS) $$(OFLAG) $$(COVFLAGS) $$(${1}_MV_${2}) $$(INCPPFLAGS)
$$(patsubst %.o,%.prof.o,$$(MV_OBJS)):  DCPPFLAGS := $$(CPPFLAG) $$(${1}_CPPFLAGS) $$(OFLAG) $$(PROFFLAGS) $(${1}_MV_${2}) $$(INCPPFLAGS)
$$(patsubst %.o,%.cov.o,$$(MV_OBJS)):  DCXXFLAGS := $$(DCXXFLAGS) $$(${1}_CXXFLAGS)
$$(patsubst %.o,%.prof.o,$$(MV_OBJS)):  DCXXFLAGS := $$(DCXXFLAGS) $$(${1}_CXXFLAGS)
$$(patsubst %.o,%.cov.o,$$(MV_OBJS)):  DCFLAGS := $$(DCFLAGS) $$(${1}_CFLAGS)
$$(patsubst %.o,%.prof.o,$$(MV_OBJS)):  DCFLAGS := $$(DCFLAGS) $$(${1}_CFLAGS)

$$(foreach EXT,$$(C_EXTS),\
	$$(eval $$(call CREAT_OBJECT_RULE,$$(${1}_BUILDDIR),%_${2}.o,$${EXT},$${C_RULE})))
$$(foreach EXT,$$(CXX_EXTS),\
	$$(eval $$(call CREAT_OBJECT_RULE,$$(${1}_BUILDDIR),%_${2}.o,$${EXT},$${CXX_RULE})))
$$(foreach EXT,$$(C_EXTS),\
	$$(eval $$(call CREAT_OBJECT_RULE,$$(${1}_BUILDDIR),%_${2}.cov.o,$${EXT},$${C_RULE})))
$$(foreach EXT,$$(CXX_EXTS),\
	$$(eval $$(call CREAT_OBJECT_RULE,$$(${1}_BUILDDIR),%_${2}.cov.o,$${EXT},$${CXX_RULE})))
$$(foreach EXT,$$(C_EXTS),\
	$$(eval $$(call CREAT_OBJECT_RULE,$$(${1}_BUILDDIR),%_${2}.prof.o,$${EXT},$${C_RULE})))
$$(foreach EXT,$$(CXX_EXTS),\
	$$(eval $$(call CREAT_OBJECT_RULE,$$(${1}_BUILDDIR),%_${2}.prof.o,$${EXT},$${CXX_RULE})))

$$(${1}_PATH): $$(MV_OBJS)
$$(${1}_COVPATH): $$(patsubst %.o,%.cov.o,$$(MV_OBJS))
$$(${1}_PROFPATH): $$(patsubst %.o,%.prof.o,$$(MV_OBJS))

else
ifeq ($$(V),1)
$$(info "Skipping ${1}_MV_${2}")
$$(info $$(FLAGSWORK))
endif
endif

${1}_MV_OBJS += $$(MV_OBJS) $$(patsubst %.o,%.cov.o,$$(MV_OBJS)) $$(patsubst %.o,%.prof.o,$$(MV_OBJS))

endef

define SET_CXX
$(if $(filter ${1},$($(2)_LIBS)),$(2)_HASCXX:=cxx,)
endef

define PUSH_CXX
ifeq ($$(${1}_HASCXX),cxx)
$$(foreach DEP,$$(ALL_TARGETS),\
	$$(eval $$(call SET_CXX,${1},$$(DEP))))
endif
endef

# Setup dependencies for each target so all sources are build into the target
define MAKE_TARGET
# Basic path and target type variable setup
${1}_BUILDDIR := $$(call CONCAT,$${DIR},${BUILDDIR})
INCFLAGS := $$(addprefix -I,$$(call CONCAT,$${DIR},$$(${1}_INCDIR)))

ifeq "$(filter %.a,${1})" ""
TDIR := $${DIR}
${1}_TYPE=prog
else
TDIR := $${LIB_DIR}
${1}_TYPE=static
endif

${1}_SRC := $$(subst $$(DIR)/,,$$(wildcard $$(call CONCAT,$$(DIR),$${${1}_SRC})))
${1}_MV_SRC := $$(subst $$(DIR)/,,$$(wildcard $$(call CONCAT,$$(DIR),$${${1}_MV_SRC})))
${1}_SRC := $$(filter-out $$(${1}_MV_SRC),$$(${1}_SRC))

# Figure correct path for the target
${1}_PATH := $$(call CONCAT,$${TDIR},${1})
# fugure out what should be build from the source file extension
${1}_OBJS := $$(call CONCAT,$$(${1}_BUILDDIR),\
	$$(filter-out %.l,$$(patsubst %.cpp,%.o,$$(patsubst %.y,%.o,\
	$$(patsubst %.c,%.o,$${${1}_SRC})))))
${1}_FLEXS := $$(call CONCAT,$$(${1}_BUILDDIR),\
	$$(patsubst %.l,%.c,$$(filter %.l,$${${1}_SRC})))
${1}_YACCS := $$(call CONCAT,$$(${1}_BUILDDIR),\
	$$(patsubst %.y,%.o,$$(filter %.y,$${${1}_SRC})))

${1}_MV_OBJS :=

# Setup shadow objects for coverage and profile targets with different flags
${1}_COVOBJS := $$(patsubst %.o,%.cov.o,$$(${1}_OBJS))
${1}_PROFOBJS := $$(patsubst %.o,%.prof.o,$$(${1}_OBJS))
${1}_PROFYACCS := $$(patsubst %.o,%.prof.o,$$(${1}_YACCS))
${1}_PROFFLEXS := $$(patsubst %.c,%.prof.c,$$(${1}_FLEXS))

# Check if we need CXX compiler
ifeq "$$(filter $$(CXX_EXTS),$$(${1}_SRC))" ""
${1}_HASCXX := c
else
${1}_HASCXX := cxx
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

$$(foreach CFG,$$(${1}_MV_CFG),\
	$$(eval $$(call MAKE_VERSIONED,${1},$${CFG})))

#Main target dependencies only if it is current directory or subdirectory
ifeq ($$(filter-out $(CURDIR)%,$$(abspath $$(DIR))),)
all: $$(${1}_PATH)
prof: $$(${1}_PROFPATH)
cov: $$(${1}_COVPATH)
endif

#Target variables
ifneq ($(filter release,$(MAKECMDGOALS)),)
$$(${1}_OBJS): DCPPFLAGS := $(OPTFLAGS) $(DCPPFLAGS) $$(${1}_CPPFLAGS) $(PUSEFLAGS) $$(INCPPFLAGS)
else
$$(${1}_OBJS): DCPPFLAGS := $(OPTFLAGS) $(DCPPFLAGS) $$(${1}_CPPFLAGS) $$(INCPPFLAGS)
endif
$$(${1}_OBJS): DCXXFLAGS := $(DCXXFLAGS) $$(${1}_CXXFLAGS)
$$(${1}_OBJS): DCFLAGS := $(DCFLAGS) $$(${1}_CFLAGS)
$$(${1}_COVOBJS): DCPPFLAGS := $(OPTFLAGS) $(DCPPFLAGS) $$(${1}_CPPFLAGS) $(COVFLAGS) $$(INCPPFLAGS)
$$(${1}_COVOBJS): DCXXFLAGS := $(DCXXFLAGS) $$(${1}_CXXFLAGS)
$$(${1}_COVOBJS): DCFLAGS := $(DCFLAGS) $$(${1}_CFLAGS)
$$(${1}_PROFOBJS): DCPPFLAGS := $(OPTFLAGS) $(DCPPFLAGS) $$(${1}_CPPFLAGS) $(PROFFLAGS) $$(INCPPFLAGS)
$$(${1}_PROFOBJS): DCXXFLAGS := $(DCXXFLAGS) $$(${1}_CXXFLAGS)
$$(${1}_PROFOBJS): DCFLAGS := $(DCFLAGS) $$(${1}_CFLAGS)
ifneq ($(filter release,$(MAKECMDGOALS)),)
$${${1}_PATH}: DCPPFLAGS := $(OPTFLAGS) $(DCPPFLAGS) $$(${1}_CPPFLAGS) $(PUSEFLAGS)
else
$${${1}_PATH}: DCPPFLAGS := $(DCPPFLAGS) $$(${1}_CPPFLAGS) $(OPTFLAGS)
endif
$$(${1}_PATH): DCXXFLAGS := $(DCXXFLAGS) $$(${1}_CXXFLAGS)
$$(${1}_PATH): DCFLAGS := $(DCFLAGS) $$(${1}_CFLAGS)
$${${1}_PROFPATH}: DCPPFLAGS := $(OPTFLAGS) $(DCPPFLAGS) $$(${1}_CPPFLAGS) $(PROFFLAGS)
$$(${1}_PROFPATH): DCXXFLAGS := $(DCXXFLAGS) $$(${1}_CXXFLAGS)
$$(${1}_PROFPATH): DCFLAGS := $(DCFLAGS) $$(${1}_CFLAGS)
$${${1}_COVPATH}: DCPPFLAGS := $(OPTFLAGS) $(DCPPFLAGS) $$(${1}_CPPFLAGS) $(COVFLAGS)
$$(${1}_COVPATH): DCXXFLAGS := $(DCXXFLAGS) $$(${1}_CXXFLAGS)
$$(${1}_COVPATH): DCFLAGS := $(DCFLAGS) $$(${1}_CFLAGS)

# Libary dependencies
# TODO support shared libraries
${1}_SYSLIBS := $$(filter -l%,$$(${1}_LIBS))
${1}_LIBS := $$(filter-out -l%,$$(${1}_LIBS))
ifeq ($$(${1}_HASCXX),c)
$$(foreach LIB,$$(${1}_LIBS),\
	$$(if $$(filter cxx,$$($${LIB}_HASCXX)),${1}_HASCXX:=cxx,))
endif
ifeq ($$(${1}_HASCXX),cxx)
$$(eval $$(call PUSH_CXX,${1}))
endif

$${${1}_PATH}: $$(call CONCAT,$$(LIB_DIR),$$(${1}_LIBS))
$${${1}_COVPATH}: $$(call CONCAT,$$(LIB_DIR),$$(subst .a,.cov.a,$$(${1}_LIBS)))
$${${1}_PROFPATH}: $$(call CONCAT,$$(LIB_DIR),$$(subst .a,.prof.a,$$(${1}_LIBS)))

# Setup flex and yacc dependencies
ifneq "$$(${1}_YACCS)" ""
$$(${1}_YACCS): $$(${1}_FLEXS) $$(subst .o,.c,$$(${1}_YACCS))
ifneq ($(filter release,$(MAKECMDGOALS)),)
$$(${1}_YACCS): DCFLAGS := $$(DCFLAGS) $$(${1}_CFLAGS) -Wno-unused-function $$(PUSEFLAGS) $$(INCFLAGS)
else
$$(${1}_YACCS): DCFLAGS := $$(DCFLAGS) $$(${1}_CFLAGS) -Wno-unused-function  $$(INCFLAGS)
endif
$$(subst .o,.cov.o,$$(${1}_YACCS)): $$(${1}_FLEXS) $$(subst .o,.c,$$(${1}_YACCS))
$$(subst .o,.cov.o,$$(${1}_YACCS)): DCFLAGS := $$(DCFLAGS) $$(${1}_CFLAGS) -Wno-unused-function $$(COVFLAGS) $$(INCFLAGS)
$$(${1}_PROFYACCS): $$(${1}_PROFFLEXS) $$(subst .o,.c,$$(${1}_PROFYACCS))
$$(${1}_PROFYACCS): DCFLAGS := $$(DCFLAGS) $$(${1}_CFLAGS) -Wno-unused-function  $$(PROFFLAGS) $$(INCFLAGS)
endif

ALLOBJS := $$(${1}_OBJS) $$(${1}_COVOBJS) $${${1}_PROFOBJS} $$(${1}_MV_OBJS)
GENFILES := $$(subst .o,.c,$$(${1}_YACCS)) $$(subst .o,.c,$$(${1}_PROFYACCS)) $$(${1}_PROFFLEXS) $$(${1}_FLEXS)

BUILDMKDEP :=
ifneq (,$(wildcard $(call CONCAT,$(TOP),build.mk)))
BUILDMKDEP += $(call CONCAT,$(TOP),build.mk)
endif

# All objets should depend on makefile changes that affect their rules
$$(ALLOBJS) $$(GENFILES): $$(call CANONICAL_PATH,$$(MKFILE) $(call CONCAT,$(TOP),main.mk Makefile) $$(BUILDMKDEP))

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
DOCS	:=
DOCS_INSTALL_PATH :=
BININST :=
PERLLIBINST :=
PERLIBNAME :=
MAN_SECTION := 6

# Keep track of current subdiretory processing
DIR := $(call CANONICAL_PATH,$(dir $(1)))
DIR_STACK := $$(call PUSH,$$(DIR_STACK),$$(DIR))

ALL_BUILDDIRS += $$(call CONCAT,$$(DIR),$(BUILDDIR))

# Time to process the makefile
include $(1)

# Generated rules for each target declared
$$(foreach TARGET, $${TARGETS},\
$$(eval $$(call MAKE_TARGET,$${TARGET}))\
)

MANS := $$(subst $$(DIR),,$$(wildcard $$(call CONCAT,$$(DIR),$$(filter %.pod,$$(DOCS)))))
MANS := $$(call CONCAT,$$(call CONCAT,$$(DIR),$(BUILDDIR)),$$(subst .pod,.$$(MAN_SECTION),$$(MANS)))

$$(call CONCAT,$$(DIR),$(BUILDDIR))/%.$$(MAN_SECTION): $$(call CONCAT,$$(DIR),%.pod)
	@mkdir -p $$(dir $$@)
	$$(SPOD2MAN) --section=6 --release="$(PROGRAM)" --center="User Documentation" $$< > $$@

ifneq ($$(abspath $$(DIR)),$$(DIR))
ifneq ($$(MANS),)
all: $$(MANS)

clean_dir_$$(call CONCAT,$$(DIR),$(BUILDDIR)): clean_man_$$(DIR)

clean_man_$$(DIR): MANS := $$(MANS)
clean_man_$$(DIR): DIR := $$(DIR)

clean_man_$$(DIR):
	$$(SRM) $$(MANS)
endif
endif

BININST := $$(call CONCAT,$$(DIR),$$(BININST))
$$(eval $$(call MAKE_BIN_INSTALL,$$(wildcard $$(BININST)),$$(DIR),))
PERLLIBINST := $$(call CONCAT,$$(DIR),$$(PERLLIBINST))
$$(eval $$(call MAKE_PERLLIB_INSTALL,$$(wildcard $$(PERLLIBINST)),$$(DIR),$$(PERLIBNAME)))
DOCS := $$(call CONCAT,$$(DIR),$$(filter-out %.pod,$$(DOCS))) 
$$(eval $$(call MAKE_DOC_INSTALL,$$(wildcard $$(DOCS)),$$(DIR),$$(DOCS_INSTALL_PATH)))
$$(eval $$(call MAKE_MAN_INSTALL,$$(MANS),$$(call CONCAT,$$(DIR),$$(BUILDDIR)),))

# Include subdiretories of subdiretories
SUBDIRS := $$(wildcard $$(call CONCAT,$$(DIR),$$(SUBDIRS)))
$$(foreach SUB, $$(SUBDIRS),\
$$(eval $$(call INCLUDE_SUBDIR,$${SUB})))

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
	$${SRM} $$(call CONCAT,$$(${1}_BUILDDIR),*.gcda *.gcno)
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
$(call CONCAT,${1},${2}): $(call CONCAT,$(call CONCAT,${1},..),${3})
	${4}

$(call CONCAT,${1},${2}): $(call CONCAT,${1},${3})
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
	$${COMPILE.cxx}
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

# Don't require configuration but use variables if defined
-include $(call CONCAT,$(TOP),build.mk)
$(eval $(call INCLUDE_SUBDIR,$(call CONCAT,$(TOP),main.mk)))

FLAGS_CLEAN := $(LIB_DIR)/__flagstest.o $(LIB_DIR)/__flagstest.d

$(eval $(call INCLUDE_SUBDIR,$(call CONCAT,$(TOP),Rules.mk)))

clean_dir_$(LIB_DIR): clean_flagstest
clean_flagstest:
	$(SRM) $(FLAGS_CLEAN)

# Always clean flagstest in-depend of subdirectory where clean was called
# because there is no idea what subdirectory these belong to
# Probably should use subdirectory build directory instead
clean: clean_flagstest

# Setup compilation for a versioned file
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

# Only add build dirs that exists
$(foreach DIR,$(wildcard $(ALL_BUILDDIRS)),\
	$(eval $(call CREAT_CLEAN_DIR_RULE,$(DIR))))

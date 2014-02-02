# Some helpers

define CANONICAL_PATH
$(subst $(CURDIR),.,$(patsubst $(CURDIR)/%,%,$(abspath $(1))))
endef

# Find main.mk in the directory tree
ifndef TOP
TOP := $(shell \
	totest=$(CURDIR); \
	while [ ! -r "$$totest/main.mk" ] && [ "$$totest" != "" ]; do \
		totest=$${totest%/*};  \
	done; \
	echo $$totest)
endif
LIB_DIR = $(call CANONICAL_PATH,${TOP}/${BUILDDIR})
TARGET_DIR := $(call CANONICAL_PATH,${TOP})
ALL_TARGETS :=
LINK.c = $(SCC) $(CFLAGS) -o $@ $^
LINK.cxx = $(SCXX) $(CFLAGS) $(CXXFLAGS) -o $@ $^
COMPILE.c = $(SCC) -c $(CFLAGS) -o $@ $<
COMPILE.cxx = $(SCXX) -c $(CFLAGS) $(CXXFLAGS) -o $@ $<
STATICLIB = $(SAR) ${ARFLAGS} $@ $?

define CREAT_TARGET_RULE
ifeq ($$(${1}_TYPE),prog)
$$(${1}_PATH): $${${1}_OBJS}
	@mkdir -p $(dir $$@)
	$$(LINK.c)

install: install_${1}

install_${1}: $$(${1}_PATH)
	/usr/bin/install -m 0755 $$(${1}_PATH) $$(BINPREFIX)/$$(notdir $$(${1}_PATH))

uninstall: uninstall_${1}

uninstall_${1}:
	$${SRM} $$(BINPREFIX)/$$(notdir $$(${1}_PATH))

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

define CREAT_CLEAN_RULE
ifneq ($(abspath $(${1}_BUILDDIR)),$(${1}_BUILDDIR))
clean: clean_${1}
endif

clean_${1}:
	$$(SRM) $$(${1}_CLEAN)
	-$$(SRMDIR) $$(${1}_BUILDDIR)
endef

define MAKE_TARGET
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
${1}_PATH := $$(call CANONICAL_PATH,$$(addprefix $${TDIR}/,${1}))
else
${1}_PATH := $$(call CANONICAL_PATH,${1})
endif
${1}_OBJS := $$(call CANONICAL_PATH,$$(addprefix $$(${1}_BUILDDIR)/,\
	$$(filter-out %.l,$$(subst .cpp,.o,$$(subst .y,.o,\
	$$(subst .c,.o,$${${1}_SRC}))))))
${1}_FLEXS := $$(call CANONICAL_PATH,$$(addprefix $$(${1}_BUILDDIR)/,\
	$$(subst .l,.c,$$(filter %.l,$${${1}_SRC}))))
${1}_YACCS := $$(call CANONICAL_PATH,$$(addprefix $$(${1}_BUILDDIR)/,\
	$$(subst .y,.o,$$(filter %.y,$${${1}_SRC}))))

${1}_COVOBJS := $$(subst .o,.cov.o,$$(${1}_OBJS))
${1}_PROFOBJS := $$(subst .o,.prof.o,$$(${1}_OBJS))
${1}_PROFYACCS := $$(subst .o,.prof.o,$$(${1}_YACCS))
${1}_PROFFLEXS := $$(subst .c,.prof.c,$$(${1}_FLEXS))

ifeq "$$(filter %.cpp,$$(${1}_SRC))" ""
${1}_HASCXX := 1
else
${1}_HASCXX := 0
endif

ALL_TARGETS += ${1}

ifeq ($$(${1}_TYPE),prog)
${1}_COVPATH := $$(addsuffix .cov,$$(${1}_PATH))
${1}_PROFPATH := $$(addsuffix .prof,$$(${1}_PATH))
else
${1}_COVPATH := $$(subst .a,.cov.a,$$(${1}_PATH))
${1}_PROFPATH := $$(subst .a,.prof.a,$$(${1}_PATH))
endif

#Main target dependencies
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

$${${1}_PATH}: $$(addprefix $$(LIB_DIR)/,$$(${1}_LIBS))
$${${1}_COVPATH}: $$(addprefix $$(LIB_DIR)/,$$(subst .a,.cov.a,$$(${1}_LIBS)))
$${${1}_PROFPATH}: $$(addprefix $$(LIB_DIR)/,$$(subst .a,.prof.a,$$(${1}_LIBS)))

ifneq "$$(${1}_YACCS)" ""
$$(${1}_YACCS): $$(${1}_FLEXS) $$(subst .o,.c,$$(${1}_YACCS))
ifneq ($(filter release,$(MAKECMDGOALS)),)
$$(${1}_YACCS): CFLAGS := $(CFLAGS) $$(${1}_CFLAGS) -Wno-unused-function $(PUSEFLAGS) $$(INCFLAGS)
else
$$(${1}_YACCS): CFLAGS := $(CFLAGS) $$(${1}_CFLAGS) -Wno-unused-function  $$(INCFLAGS)
endif
$$(subst .o,.cov.o,$$(${1}_YACCS)): $$(${1}_FLEXS) $$(subst .o,.c,$$(${1}_YACCS))
$$(subst .o,.cov.o,$$(${1}_YACCS)): CFLAGS := $(CFLAGS) $$(${1}_CFLAGS) -Wno-unused-function $(COVFLAGS)
$$(${1}_PROFYACCS): $$(${1}_PROFFLEXS) $$(subst .o,.c,$$(${1}_PROFYACCS))
$$(${1}_PROFYACCS): CFLAGS := $(CFLAGS) $$(${1}_CFLAGS) -Wno-unused-function  $(PROFFLAGS)
endif

$$(${1}_OBJS): $$(call CANONICAL_PATH,$$(MKFILE) $(TOP)/main.mk $(TOP)/Makefile)

ALLOBJS := $$(${1}_OBJS) $$(${1}_COVOBJS) $${${1}_PROFOBJS}

-include $$(subst .o,.d,$$(ALLOBJS))

${1}_CLEAN := $$(${1}_PATH) $$(${1}_COVPATH) $$(${1}_PROFPATH) $$(ALLOBJS) $$(subst .o,.d,$$(ALLOBJS)) $$(subst .o,.c,$$(${1}_YACCS)) $$(${1}_FLEXS) $$(subst .o,.gcda,$$(ALLOBJS)) $$(subst .o,.gcno,$$(ALLOBJS))

endef

define INCLUDE_SUBDIR
TARGETS :=
SUBDIRS :=
MKFILE  := $(1)

DIR := $(call CANONICAL_PATH,$(dir $(1)))
DIR_STACK := $$(call PUSH,$$(DIR_STACK),$$(DIR))


include $(1)

$$(foreach TARGET, $${TARGETS},\
$$(eval $$(call MAKE_TARGET,$${TARGET}))\
)
ifneq "$$(DIR)" ""
$$(foreach SUB, $$(SUBDIRS),\
$$(eval $$(call INCLUDE_SUBDIR,$$(addprefix $$(DIR)/,$${SUB}))))
else
$$(foreach SUB, $$(SUBDIRS),\
$$(eval $$(call INCLUDE_SUBDIR,$${SUB})))
endif


DIR_STACK := $$(call POP,$$(DIR_STACK))
DIR := $$(call PEEK,$$(DIR_STACK))
endef

define MIN
$(firstword $(sort $(1) $(2)))
endef

define PEEK
$(lastword $(subst :, ,$(1)))
endef

define POP
$(1:%:$(lastword $(subst :, ,$(1)))=%)
endef

define PUSH
$(2:%=$(1):%)
endef

define PROFILE_DEPEND
$$(${1}_OBJS): $$(subst .o,.gcda,$$(${1}_OBJS))
$$(subst .o,.gcda,$$(${1}_OBJS)): profiletest
endef

define PROFOBJS_DEPEND_CLEAN
$$(${1}_PROFOBJS): clean_gcda
$$(${1}_PROFFLEXS): clean_gcda
$$(subst .o,.c,$$(${1}_PROFYACCS)): clean_gcda

clean_gcda: clean_${1}_gcda

clean_${1}_gcda:
	$${SRM} $$(${1}_BUILDDIR)/*.gcda $$(${1}_BUILDDIR)/*.gcno
endef

release: all
	@echo Target $@ done!

all:
	@echo Target $@ done!

cov:
	@echo Target $@ done!

prof:
	@echo Target $@ done!

define CREAT_OBJECT_RULE
${1}/${2}: $(call CANONICAL_PATH,${1}/../${3})
	${4}

${1}/${2}: ${1}/${3}
	${4}
endef

define C_RULE
	@mkdir -p $$(dir $$@)
	$${COMPILE.c}
endef

define CXX_RULE
	@mkdir -p $$(dir $$@)
	$${COMPILE.c}
endef

define YACC_RULE
	@mkdir -p $$(dir $$@)
	$$(SYACC) $$< -o $$@
endef

define PROFYACC_RULE
	@mkdir -p $$(dir $$@)
	$$(SYACC) $$< -o $$@
	@sed -i 's/scan.c/scan.prof.c/' $$@
endef

define FLEX_RULE
	@mkdir -p $$(dir $$@)
	$$(SFLEX) -o $$@ $$<
endef

C_EXTS := %.c
FLEX_EXTS := %.l
YACC_EXTS := %.y
CXX_EXTS := %.cpp %.cxx

$(eval $(call INCLUDE_SUBDIR,$(call CANONICAL_PATH,$(TOP)/main.mk)))
$(eval $(call INCLUDE_SUBDIR,$(call CANONICAL_PATH,$(TOP)/Rules.mk)))

profiletest: ${ALLTESTS}
	@find -name "*.gcda" | sed 's/^\(.*\).prof\(.*\)$$/mv \0 \1\2/' | $(SHELL)

$(foreach TARGET,${ALL_TARGETS},\
	$(eval $(call PROFOBJS_DEPEND_CLEAN,${TARGET})))

ifneq ($(filter release,$(MAKECMDGOALS)),)
$(foreach TARGET,${ALL_TARGETS},\
	$(eval $(call PROFILE_DEPEND,${TARGET})))
endif

$(foreach TARGET,${ALL_TARGETS},\
	$(eval $(call CREAT_TARGET_RULE,${TARGET})))

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

$(foreach TARGET,${ALL_TARGETS},\
	$(eval $(call CREAT_CLEAN_RULE,${TARGET})))

ifdef OLDMAKE
TARFILE  = $(PROGRAM).tar
GZIPFILE = $(PROGRAM).tar.gz

OBJ  = $(addprefix $(BUILDDIR)/, dealer.o defs.o pbn.o c4.o pointcount.o)
OBJCOV = $(subst .o,.cov.o,$(OBJ))

GENOBJ  = $(addprefix $(BUILDDIR)/, $(subst .c,.o,$(GENSRC)))

all: $(PROGRAM) $(GENLIB)

$(PROGRAM): $(OBJ)
	$(SCC) $(CFLAGS) -o $@ $^

$(PROGRAMCOV): $(OBJCOV)
	$(SCC) $(CFLAGS) $(COVFLAGS) -o $@ $^

$(GENLIB): $(GENOBJ)
	$(SCC) $(CFLAGS) -o $@ $^

clean:
	@rm -f $(OBJ) $(OBJCOV) $(LOBJ) $(YOBJ) $(subst .o,.d,$(OBJ) $(OBJCOV)) $(BUILDDIR)/deb dealer
	@rm -f $(BUILDDIR)/*.gcda $(BUILDDIR)/*.gcno
	@[ -e $(BUILDDIR) ] && rmdir $(BUILDDIR) || true

tarclean: clean $(YOBJ)
	rm -f dealer
	rm -f $(TARFILE)  $(GZIPFILE)

tarfile: tarclean
	cd .. ; \
	tar cvf $(TARFILE) $(PROGRAM) --exclude CVS --exclude $(TARFILE); \
	mv $(TARFILE) $(PROGRAM) 
	gzip -f $(TARFILE)

#
# Lex
#
$(BUILDDIR)/%.c: %.l $(BUILDDIR)/deb
	$(SFLEX) -o $@ $<

#
# Yacc
#
$(BUILDDIR)/%.c: %.y $(BUILDDIR)/deb
	$(SYACC) $< -o $@

#
# C-code
#
$(BUILDDIR)/%.o: %.c $(BUILDDIR)/deb
	$(SCC) -c $(CFLAGS) -o $@ $<

$(BUILDDIR)/%.o: $(BUILDDIR)/%.c $(BUILDDIR)/deb
	$(SCC) -c $(CFLAGS) -Wno-unused-function -o $@ $<

$(BUILDDIR)/%.cov.o: %.c $(BUILDDIR)/deb
	$(SCC) -c $(CFLAGS) $(COVFLAGS) -o $@ $<

$(BUILDDIR)/%.cov.o: $(BUILDDIR)/%.c $(BUILDDIR)/deb
	$(SCC) -c $(CFLAGS) -Wno-unused-function $(COVFLAGS) -o $@ $<

#
# BUILDDIR
#
$(BUILDDIR)/deb: Makefile $(wildcard */Makefile) Makefile.vars
	$(SMKDIR) $(BUILDDIR)
	@touch $@

# 
# File dependencies
#
$(BUILDDIR)/scan.c: scan.l
$(BUILDDIR)/defs.c: defs.y $(BUILDDIR)/scan.c
$(BUILDDIR)/defs.o: $(BUILDDIR)/scan.c

include Random/Makefile
include Examples/Makefile
-include $(OBJS:.o=.d)

endif

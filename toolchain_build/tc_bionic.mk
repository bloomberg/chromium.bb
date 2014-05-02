# Copyright (c) 2014 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

#
# GNU Make based build file.  For details on GNU Make see:
#   http://www.gnu.org/software/make/manual/make.html
#

all:

MAKEFILE_DEPS?=$(MAKEFILE_LIST)

#
# Rules for output directories.
#
# Output will be places in a directory name based on Toolchain and configuration
# be default this will be "newlib/Debug".  We use a python wrapped MKDIR to
# proivde a cross platform solution. The use of '|' checks for existance instead
# of timestamp, since the directory can update when files change.
#
%dir.stamp :
	mkdir -p $(dir $@)
	@echo Directory Stamp > $@

#
# Define a LOG macro that allow a command to be run in quiet mode where
# the command echoed is not the same as the actual command executed.
# The primary use case for this is to avoid echoing the full compiler
# and linker command in the default case.  Defining V=1 will restore
# the verbose behavior
#
# $1 = The name of the tool being run
# $2 = The target file being built
# $3 = The full command to run
#
ifdef V
define LOG
$(3)
endef
else
ifeq ($(OSNAME),win)
define LOG
@echo   $(1) $(2) && $(3)
endef
else
define LOG
@echo "  $(1) $(2)" && $(3)
endef
endif
endif

#
# BASIC Macros
#
# $1 = Output (fully qualified path)
# $2 = Input (fully qualified path)
# $3 = Tool (fully qualified path)
# $4 = PRE ARGS (before output name)
# $5 = STD ARGS (before input name)
# $6 = POST ARGS (final args)
# $7 = Extra Required DEPS
# $8 = Add to Depend list
# $9 = GROUP Name (compile, lib, link, etc...)
#
# The standard definition is:
# $(1) : $(2) $(7)
#    $(3) $(4) $(1) $(5) $(2) $(6)
#
# a.o : a.c Makefile
#    gcc -o a.o -c a.c $(CCFLAGS)
#
define BASIC_TARGET
$(9) : $(1)
$(9)_info : $(1)_info
$(1)_info :
	@echo TARGET: $(1)
	@echo REQUIRES: $(2)
ifdef V
	@echo TOOL: $(3)
	@echo PRE ARGS: $(4)
	@echo STD ARGS: $(5)
	@echo POST ARGS: $(6)
endif
	@echo DEPENDS ON: $(7)
	@echo DEPEND FOR: $(8)
	@echo

ifneq (,$(8))
$(8) += $(1)
endif
endef

define CLEAN_TARGET
.PHONY: $(1)_clean
clean: $(1)_clean
$(2)_clean: $(1)_clean

$(1)_clean:
	rm -fr $(1)
endef

.PHONY : compile compile_info
#
# Basic Compile Target
#
define BASIC_COMPILE_TARGET
$(call BASIC_TARGET,$(1),$(2),$(3),$(4),$(5),$(6),$(7) $(MAKEFILE_DEPS),$(8),compile)

-include $(patsubst %.o,%.d,$(1))
$(1) : $(2) $(7) $(MAKEFILE_DEPS)
	mkdir -p $(dir $(1))
	$(call LOG,$(notdir $(3)),$(1),$(3) $(4) -o $(1) -c $(2) $(5) $(6))

ifdef E
$(basename $(notdir $(1))).txt : $(2) $(7) $(MAKEFILE_DEPS)
	$(call LOG,$(notdir $(3) -E),$(basename $(notdir $(1))).txt,$(3) $(4) -o $(basename $(notdir $(1))).txt -E $(2) $(5) $(6))
endif
endef

.PHONY : lib lib_info lib_clean
#
# Basic Libarary Target
#
define BASIC_LIB_TARGET
$(call BASIC_TARGET,$(1),$(2),$(3),$(4),$(5),$(6),$(7),$(8),lib)
$(call CLEAN_TARGET,$(1),lib)

all: $(1)
$(1): $(2) $(7) | $(dir $(1))dir.stamp
	$(RM) $(1)
	$(call LOG,LIB ,$(1),$(3) $(4) -cr $(1) $(2) $(5) $(6))
endef

#
# Basic Link Target
#
.PHONY : link link_info link_clean
define BASIC_LINK_TARGET
$(call BASIC_TARGET,$(1),$(2),$(3),$(4),$(5),$(6),$(7),$(8),link)
$(call CLEAN_TARGET,$(1),link)

all: $(1)
$(1): $(2) $(7) | $(dir $(1))dir.stamp
	$(call LOG,LINK,$(1),$(3) $(4) -o $(1) $(2) $(5) $(6))
endef

#
# Basic Install Target
#
.PHONY : install_info install_clean
define BASIC_INSTALL_TARGET
$(call BASIC_TARGET,$(1),$(2),$(3),$(4),$(5),$(6),$(7),$(8),install)
$(call CLEAN_TARGET,$(1),install)

all: $(1)
$(1): $(2) $(7) | $(dir $(1))dir.stamp
	$(call LOG,CP,$(1),$(3) $(4) $(5) $(2) $(1) $(6))
endef

.PHONY : stamp_info stamp_clean
#
# Basic Stamp Target
#
define BASIC_STAMP_TARGET
$(call BASIC_TARGET,$(1),$(2),$(3),$(4),$(5),$(6),$(7),$(8),stamp)
$(call CLEAN_TARGET,$(1),stamp)

all: $(1)
install: $(1)
$(1): $(2) $(7) | $(dir $(1))dir.stamp
	$(call LOG,STAMP,$(1),$(3) $(4) $(5) $(6) > $(1))
endef


#
# Compile Macro
#
# $1 = Src File
# $2 = Flags
# $3 = Output Dir
# $4 = Extra Required DEPS
# $5 = Add to Depend list
define C_COMPILER_RULE
ifneq (,$(findstring x86_32,$(ARCHES)))
$(call BASIC_COMPILE_TARGET,$(call SRC_TO_OBJ,$(1),_x86_32,$(3)),$(1),$(X86_32_CC),$(X86_INCS) $(POSIX_FLAGS),$(2),$(X86_32_CFLAGS) $(X86_32_CCFLAGS),$(4),$(5))
endif

ifneq (,$(findstring x86_64,$(ARCHES)))
$(call BASIC_COMPILE_TARGET,$(call SRC_TO_OBJ,$(1),_x86_64,$(3)),$(1),$(X86_64_CC),$(X86_INCS) $(POSIX_FLAGS),$(2),$(X86_64_CFLAGS) $(X86_64_CCFLAGS),$(4),$(5))
endif

ifneq (,$(findstring arm,$(ARCHES)))
$(call BASIC_COMPILE_TARGET,$(call SRC_TO_OBJ,$(1),_arm,$(3)),$(1),$(ARM_CC),$(ARM_INCS) $(POSIX_FLAGS),$(2),$(ARM_CFLAGS) $(ARM_CCFLAGS),$(4),$(5))
endif
endef

define CXX_COMPILER_RULE
ifneq (,$(findstring x86_32,$(ARCHES)))
$(call BASIC_COMPILE_TARGET,$(call SRC_TO_OBJ,$(1),_x86_32,$(3)),$(1),$(X86_32_CXX),$(X86_INCS) $(POSIX_FLAGS),$(2),$(X86_32_CFLAGS) $(X86_32_CXXFLAGS),$(4),$(5))
endif

ifneq (,$(findstring x86_64,$(ARCHES)))
$(call BASIC_COMPILE_TARGET,$(call SRC_TO_OBJ,$(1),_x86_64,$(3)),$(1),$(X86_64_CXX),$(X86_INCS) $(POSIX_FLAGS),$(2),$(X86_64_CFLAGS) $(X86_64_CXXFLAGS),$(4),$(5))
endif

ifneq (,$(findstring arm,$(ARCHES)))
$(call BASIC_COMPILE_TARGET,$(call SRC_TO_OBJ,$(1),_arm,$(3)),$(1),$(ARM_CXX),$(ARM_INCS) $(POSIX_FLAGS),$(2),$(ARM_CFLAGS) $(ARM_CXXFLAGS),$(4),$(5))
endif
endef

#
# $1 = Source Name
# $2 = POSIX Compile Flags
# $3 = Output Path
# $4 = Extra Required DEPS
# $5 = Add to Depend list
#
define COMPILE_RULE
ifeq ($(suffix $(1)),.c)
$(call C_COMPILER_RULE,$(1),$(2),$(3),$(4),$(5))
else
$(call CXX_COMPILER_RULE,$(1),$(2),$(3),$(4),$(5))
endif
endef


#
# INSTALL AS
#
# $1 = Destination
# $2 = Source
# $3 = Extra Required DEPS
# $4 = Add to Depend list
#
define INSTALL_RULE
$(call BASIC_INSTALL_TARGET,$(1),$(2),cp,,,,$(3),$(4))
endef

#
# INSTALL LIB
#
# $1 = Target Name
# $2 = Ext
# $3 = Input Dir
# $4 = Extra Required DEPS
# $5 = Defines target
define INSTALL_LIB
ifneq (,$(findstring x86_32,$(ARCHES)))
$(call INSTALL_RULE,$(LIBDIR)/nacl_x86_32/$(CONFIG)/$(1).$(2),$(3)/$(1)_x86_32.$(2),$(4),$(5))
endif

ifneq (,$(findstring x86_64,$(ARCHES)))
$(call INSTALL_RULE,$(LIBDIR)/nacl_x86_64/$(CONFIG)/$(1).$(2),$(3)/$(1)_x86_64.$(2),$(4),$(5))
endif

ifneq (,$(findstring arm,$(ARCHES)))
$(call INSTALL_RULE,$(LIBDIR)/nacl_arm/$(CONFIG)/$(1).$(2),$(3)/$(1)_arm.$(2),$(4),$(5))
endif
endef

#
# INSTALL LIB AS
#
# $1 = Dest Name
# $2 = Src Path
# $3 = Src Name
# $4 = Src Ext
# $5 = Extra Required DEPS
# $6 = Add to Depend list
define INSTALL_LIB_AS
ifneq (,$(findstring x86_32,$(ARCHES)))
$(call INSTALL_RULE,$(LIBDIR)/nacl_x86_32/$(CONFIG)/$(1),$(2)/$(3)_x86_32.$(4),$(5),$(6))
endif

ifneq (,$(findstring x86_64,$(ARCHES)))
$(call INSTALL_RULE,$(LIBDIR)/nacl_x86_64/$(CONFIG)/$(1),$(2)/$(3)_x86_64.$(4),$(5),$(6))
endif

ifneq (,$(findstring arm,$(ARCHES)))
$(call INSTALL_RULE,$(LIBDIR)/nacl_arm/$(CONFIG)/$(1),$(2)/$(3)_arm.$(4),$(5),$(6))
endif
endef

#
# LIB Macro
#
# $1 = Target Name
# $2 = List of Sources
# $3 = Output Dir
# $4 = Required DEPS
# $5 = Defined DEP
define LIB_RULE_X86_32
ifneq (,$(findstring x86_32,$(ARCHES)))
$(call BASIC_LIB_TARGET,$(3)/lib$(1)_x86_32.a,$(foreach src,$(2),$(call SRC_TO_OBJ,$(src),_x86_32,$(3))),$(X86_32_LIB),,,,$(4),$(5))
endif
endef

define LIB_RULE_X86_64
ifneq (,$(findstring x86_64,$(ARCHES)))
$(call BASIC_LIB_TARGET,$(3)/lib$(1)_x86_64.a,$(foreach src,$(2),$(call SRC_TO_OBJ,$(src),_x86_64,$(3))),$(X86_64_LIB),,,,$(4),$(5))
endif
endef

define LIB_RULE_ARM
ifneq (,$(findstring arm,$(ARCHES)))
$(call BASIC_LIB_TARGET,$(3)/lib$(1)_arm.a,$(foreach src,$(2),$(call SRC_TO_OBJ,$(src),_arm,$(3))),$(ARM_LIB),,,,$(4),$(5))
endif
endef

define LIB_RULE
$(call LIB_RULE_X86_32,$(1),$(2),$(3),$(4),$(5))
$(call LIB_RULE_X86_64,$(1),$(2),$(3),$(4),$(5))
$(call LIB_RULE_ARM,$(1),$(2),$(3),$(4),$(5))
endef

#DEFAULT_STATIC_LIBS ?=-lsupc++ -lc -lgcc_eh
#DEFAULT_STATIC_DEPS ?=libsupc++.a libc.a libgcc_eh.a crtbegin_st.o crtend_st.o

#DEFAULT_DYNAMIC_LIBS ?=-lsupc++ -lc -lgcc_s
#DEFAULT_DYNAMIC_DEPS ?=libsupc++.so libc.so libgcc_s.so crtbegin_dy.o crtend_dy.o

#DEFAULT_SO_LIBS ?=-lsupc++ -lc -lgcc_s -lgcc
#DEFAULT_SO_DEPS ?=libsupc++.so libc.so libgcc_s.so crtbegin_so.o crtend_so.o

DEFAULT_STATIC_LIBS ?=-lsupc++ -lc -lgcc_eh
DEFAULT_STATIC_DEPS ?=libsupc++.a libc.a crtbegin_st.o crtend_st.o

DEFAULT_DYNAMIC_LIBS ?=-lsupc++ -lc -lgcc_s 
DEFAULT_DYNAMIC_DEPS ?=libsupc++.so libc.so libgcc_s.so crtbegin_dy.o crtend_dy.o

DEFAULT_SO_LIBS ?=-lsupc++ -lc -lgcc_s
DEFAULT_SO_DEPS ?=libsupc++.so libc.so libgcc_s.so crtbegin_so.o crtend_so.o

LDFLAGS_STATIC ?= -nostdlib -nostartfiles -static
LDFLAGS_DYNAMIC ?= -nostdlib -nostartfiles -Wl,-Ttext-segment,0x1000000 -Wl,--hash-style=sysv
LDFLAGS_SO ?= -nostdlib -nostartfiles -shared -fPIC -Wl,--hash-style=sysv -g

X86_32_LD ?= -L$(X86_32_LIB_PATH)
X86_64_LD ?= -L$(X86_64_LIB_PATH)
ARM_LD ?= -L$(ARM_LIB_PATH) -mcpu=cortex-a15 -mfloat-abi=hard -mthumb-interwork -Wl,-z,noexecstack -Wl,--fix-cortex-a8 -Wl,-Bsymbolic -Wl,--warn-shared-textrel

X86_32_LD_HEAD_STATIC ?= $(LIBDIR)/nacl_x86_32/$(CONFIG)/crtbegin_st.o
X86_64_LD_HEAD_STATIC ?= $(LIBDIR)/nacl_x86_64/$(CONFIG)/crtbegin_st.o
ARM_LD_HEAD_STATIC ?= $(LIBDIR)/nacl_arm/$(CONFIG)/crtbegin_st.o

X86_32_LD_HEAD_DYNAMIC ?= $(LIBDIR)/nacl_x86_32/$(CONFIG)/crtbegin_dy.o
X86_64_LD_HEAD_DYNAMIC ?= $(LIBDIR)/nacl_x86_64/$(CONFIG)/crtbegin_dy.o
ARM_LD_HEAD_DYNAMIC ?= $(LIBDIR)/nacl_arm/$(CONFIG)/crtbegin_dy.o

X86_32_LD_HEAD_SO ?= $(LIBDIR)/nacl_x86_32/$(CONFIG)/crtbegin_so.o
X86_64_LD_HEAD_SO ?= $(LIBDIR)/nacl_x86_64/$(CONFIG)/crtbegin_so.o
ARM_LD_HEAD_SO ?= $(LIBDIR)/nacl_arm/$(CONFIG)/crtbegin_so.o

ARM_LIBGCC_A ?= $(ARM_TC)/lib/gcc/arm-nacl/4.8.2/libgcc.a
X86_32_LIBGCC_A ?= $(X86_TC)/lib/gcc/x86_64-nacl/4.4.3/32/libgcc.a
X86_64_LIBGCC_A ?= $(X86_TC)/lib/gcc/x86_64-nacl/4.4.3/libgcc.a

X86_32_LD_TAIL_STATIC ?=  -T $(NACL_SDK_ROOT)/arch-x86_32/lib/static.lds $(DEFAULT_STATIC_LIBS) -lgcc_eh $(X86_32_LIBGCC_A) $(LIBDIR)/nacl_x86_32/$(CONFIG)/crtend_st.o
X86_64_LD_TAIL_STATIC ?= -T $(NACL_SDK_ROOT)/arch-x86_64/lib/static.lds $(DEFAULT_STATIC_LIBS) -lgcc_eh $(X86_64_LIBGCC_A) $(LIBDIR)/nacl_x86_64/$(CONFIG)/crtend_st.o
ARM_LD_TAIL_STATIC ?= -T $(NACL_SDK_ROOT)/arch-arm/lib/static.lds $(DEFAULT_STATIC_LIBS) $(ARM_LIBGCC_A) $(LIBDIR)/nacl_arm/$(CONFIG)/crtend_st.o

X86_32_LD_TAIL_DYNAMIC ?=  $(DEFAULT_DYNAMIC_LIBS) $(X86_32_LIBGCC_A) $(LIBDIR)/nacl_x86_32/$(CONFIG)/crtend_dy.o
X86_64_LD_TAIL_DYNAMIC ?= $(DEFAULT_DYNAMIC_LIBS) $(X86_64_LIBGCC_A) $(LIBDIR)/nacl_x86_64/$(CONFIG)/crtend_dy.o
ARM_LD_TAIL_DYNAMIC ?= $(DEFAULT_DYNAMIC_LIBS) $(ARM_LIBGCC_A) $(LIBDIR)/nacl_arm/$(CONFIG)/crtend_dy.o

X86_32_LD_TAIL_SO ?= $(DEFAULT_SO_LIBS) $(X86_32_LIBGCC_A) $(LIBDIR)/nacl_x86_32/$(CONFIG)/crtend_so.o
X86_64_LD_TAIL_SO ?= $(DEFAULT_SO_LIBS) $(X86_64_LIBGCC_A) $(LIBDIR)/nacl_x86_64/$(CONFIG)/crtend_so.o
ARM_LD_TAIL_SO ?= $(DEFAULT_SO_LIBS) $(ARM_LIBGCC_A) $(LIBDIR)/nacl_arm/$(CONFIG)/crtend_so.o

# SO Macro
#
# As well as building and installing a shared library this rule adds dependencies
# on the library's .stamp file in STAMPDIR.  However, the rule for creating the stamp
# file is part of LIB_RULE, so users of the DEPS system are currently required to
# use the LIB_RULE macro as well as the SO_RULE for each shared library.
#
# $1 = Target Name
# $2 = List of Sources
# $3 = Args
# $4 = Output Dir
# $5 = Required DEPS
# $6 = Defined DEPS
define SO_LINK_RULE_X86_32
ifneq (,$(findstring x86_32,$(ARCHES)))
$(call BASIC_LINK_TARGET,$(4)/lib$(1)_x86_32.so,$(foreach src,$(2),$(call SRC_TO_OBJ,$(src),_x86_32,$(4))),$(X86_32_LINK),$(LDFLAGS_SO) $(X86_32_LD) $(X86_32_LD_HEAD_SO),$(3),$(X86_32_LD_TAIL_SO),$(5),$(6))
endif
endef

define SO_LINK_RULE_X86_64
ifneq (,$(findstring x86_64,$(ARCHES)))
$(call BASIC_LINK_TARGET,$(4)/lib$(1)_x86_64.so,$(foreach src,$(2),$(call SRC_TO_OBJ,$(src),_x86_64,$(4))),$(X86_64_LINK),$(LDFLAGS_SO) $(X86_64_LD) $(X86_64_LD_HEAD_SO),$(3),$(X86_64_LD_TAIL_SO),$(5),$(6))
endif
endef

define SO_LINK_RULE_ARM
ifneq (,$(findstring arm,$(ARCHES)))
$(call BASIC_LINK_TARGET,$(4)/lib$(1)_arm.so,$(foreach src,$(2),$(call SRC_TO_OBJ,$(src),_arm,$(4))),$(ARM_LINK),$(LDFLAGS_SO) $(ARM_LD) $(ARM_LD_HEAD_SO),$(3),$(ARM_LD_TAIL_SO),$(5),$(6))
endif
endef

define SO_LINK_RULE
$(call SO_LINK_RULE_X86_32,$(1),$(2),$(3),$(4),$(5) $(foreach src,$(DEFAULT_SO_DEPS),$(LIBDIR)/nacl_x86_32/$(CONFIG)/$(src)),$(6))
$(call SO_LINK_RULE_X86_64,$(1),$(2),$(3),$(4),$(5) $(foreach src,$(DEFAULT_SO_DEPS),$(LIBDIR)/nacl_x86_64/$(CONFIG)/$(src)),$(6))
$(call SO_LINK_RULE_ARM,$(1),$(2),$(3),$(4),$(5) $(foreach src,$(DEFAULT_SO_DEPS),$(LIBDIR)/nacl_arm/$(CONFIG)/$(src)),$(6))
endef


#
# Specific Link Macro
#
# $1 = Target Name
# $2 = List of Sources
# $3 = Args
# $4 = Output Dir
# $5 = DEPS
#
define STATIC_LINK_RULE_X86_32
ifneq (,$(findstring x86_32,$(ARCHES)))
$(call BASIC_LINK_TARGET,$(4)/$(1)_x86_32.nexe,$(foreach src,$(2),$(call SRC_TO_OBJ,$(src),_x86_32,$(4))),$(X86_32_LINK), $(LDFLAGS_STATIC) $(X86_32_LD) $(X86_32_LD_HEAD_STATIC),$(3),$(X86_32_LD_TAIL_STATIC),$(5),$(6))
endif
endef

define STATIC_LINK_RULE_X86_64
ifneq (,$(findstring x86_64,$(ARCHES)))
$(call BASIC_LINK_TARGET,$(4)/$(1)_x86_64.nexe,$(foreach src,$(2),$(call SRC_TO_OBJ,$(src),_x86_64,$(4))),$(X86_64_LINK), $(LDFLAGS_STATIC) $(X86_64_LD) $(X86_64_LD_HEAD_STATIC),$(3),$(X86_64_LD_TAIL_STATIC),$(5),$(6))
endif
endef

define STATIC_LINK_RULE_ARM
ifneq (,$(findstring arm,$(ARCHES)))
$(call BASIC_LINK_TARGET,$(4)/$(1)_arm.nexe,$(foreach src,$(2),$(call SRC_TO_OBJ,$(src),_arm,$(4))),$(ARM_LINK), $(LDFLAGS_STATIC) $(ARM_LD) $(ARM_LD_HEAD_STATIC),$(3),$(ARM_LD_TAIL_STATIC),$(5),$(6))
endif
endef

define STATIC_LINK_RULE
$(call STATIC_LINK_RULE_X86_32,$(1),$(2),$(3),$(4),$(5))
$(call STATIC_LINK_RULE_X86_64,$(1),$(2),$(3),$(4),$(5))
$(call STATIC_LINK_RULE_ARM,$(1),$(2),$(3),$(4),$(5))
endef

#
# Dynamic Link Macro
#
# $1 = Target Name
# $2 = List of Sources
# $3 = Args
# $4 = Output Dir
# $5 = DEPS
#
define DYNAMIC_LINK_RULE_X86_32
ifneq (,$(findstring x86_32,$(ARCHES)))
$(call BASIC_LINK_TARGET,$(4)/$(1)_x86_32.nexe,$(foreach src,$(2),$(call SRC_TO_OBJ,$(src),_x86_32,$(4))),$(X86_32_LINK), $(LDFLAGS_DYNAMIC) $(X86_32_LD) $(X86_32_LD_HEAD_DYNAMIC),$(3),$(X86_32_LD_TAIL_DYNAMIC),$(5),$(6))
endif
endef

define DYNAMIC_LINK_RULE_X86_64
ifneq (,$(findstring x86_64,$(ARCHES)))
$(call BASIC_LINK_TARGET,$(4)/$(1)_x86_64.nexe,$(foreach src,$(2),$(call SRC_TO_OBJ,$(src),_x86_64,$(4))),$(X86_64_LINK), $(LDFLAGS_DYNAMIC) $(X86_64_LD) $(X86_64_LD_HEAD_DYNAMIC),$(3),$(X86_64_LD_TAIL_DYNAMIC),$(5),$(6))
endif
endef

define DYNAMIC_LINK_RULE_ARM
ifneq (,$(findstring arm,$(ARCHES)))
$(call BASIC_LINK_TARGET,$(4)/$(1)_arm.nexe,$(foreach src,$(2),$(call SRC_TO_OBJ,$(src),_arm,$(4))),$(ARM_LINK), $(LDFLAGS_DYNAMIC) $(ARM_LD) $(ARM_LD_HEAD_DYNAMIC),$(3),$(ARM_LD_TAIL_DYNAMIC),$(5),$(6))
endif
endef

define DYNAMIC_LINK_RULE
$(call DYNAMIC_LINK_RULE_X86_32,$(1),$(2),$(3),$(4),$(5))
$(call DYNAMIC_LINK_RULE_X86_64,$(1),$(2),$(3),$(4),$(5))
$(call DYNAMIC_LINK_RULE_ARM,$(1),$(2),$(3),$(4),$(5))
endef


#
# Strip Macro for each arch (e.g., each arch supported by LINKER_RULE).
#
# $1 = Target Name
# $2 = Source Name
#
define STRIP_RULE_x86_32
ifneq (,$(findstring x86_32,$(ARCHES)))
all: $(OUTDIR)/$(1)_x86_32.nexe
$(OUTDIR)/$(1)_x86_32.nexe: $(OUTDIR)/$(2)_x86_32.nexe
	$(call LOG,STRIP,$$@,$(X86_32_STRIP) -o $$@ $$^)
endif
endef

define STRIP_RULE_x86_64
ifneq (,$(findstring x86_64,$(ARCHES)))
all: $(OUTDIR)/$(1)_x86_64.nexe
$(OUTDIR)/$(1)_x86_64.nexe: $(OUTDIR)/$(2)_x86_64.nexe
	$(call LOG,STRIP,$$@,$(X86_64_STRIP) -o $$@ $$^)
endif
endef

define STRIP_RULE_ARM
ifneq (,$(findstring arm,$(ARCHES)))
all: $(OUTDIR)/$(1)_arm.nexe
$(OUTDIR)/$(1)_arm.nexe: $(OUTDIR)/$(2)_arm.nexe
	$(call LOG,STRIP,$$@,$(ARM_STRIP) -o $$@ $$^)
endif
endef

define STRIP_ALL_RULE
$(call STRIP_RULE_X86_32,$(1),$(2))
$(call STRIP_RULE_X86_64,$(1),$(2))
$(call STRIP_RULE_ARM,$(1),$(2))
endef


#
# Top-level Strip Macro
#
# $1 = Target Basename
# $2 = Source Basename
#
define STRIP_RULE
$(call STRIP_ALL_RULE,$(1),$(2))
endef

#
# Strip Macro for each arch (e.g., each arch supported by MAP_RULE).
#
# $1 = Target Name
# $2 = Source Name
#
define MAP_ALL_RULE
ifneq (,$(findstring x86_32,$(ARCHES)))
all: $(OUTDIR)/$(1)_x86_32.map
$(OUTDIR)/$(1)_x86_32.map: $(OUTDIR)/$(2)_x86_32.nexe
	$(call LOG,MAP,$$@,$(X86_32_NM) -l $$^ > $$@)
endif

ifneq (,$(findstring x86_64,$(ARCHES)))
all: $(OUTDIR)/$(1)_x86_64.map
$(OUTDIR)/$(1)_x86_64.map: $(OUTDIR)/$(2)_x86_64.nexe
	$(call LOG,MAP,$$@,$(X86_64_NM) -l $$^ > $$@)
endif

ifneq (,$(findstring arm,$(ARCHES)))
all: $(OUTDIR)/$(1)_arm.map
$(OUTDIR)/$(1)_arm.map: $(OUTDIR)/$(2)_arm.nexe
	$(call LOG,MAP,$$@,$(ARM_NM) -l $$^ > $$@ )
endif
endef

#
# Top-level MAP Generation Macro
#
# $1 = Target Basename
# $2 = Source Basename
#
define MAP_RULE
$(call MAP_ALL_RULE,$(1),$(2))
endef

#
# STAMP Rule
#
# $1 = Target Basename
# $2 = Source Basename
# $3 = Message
# $4 = Required DEPS
# $5 = Defined DEPS
#
define STAMP_RULE
$(call BASIC_STAMP_TARGET,$(1),$(2),echo,$(3),$(4),$(5))
endef

info: compile_info lib_info link_info
	@echo TOOLCHAIN=$(TOOLCHAIN)
	@echo CONFIG=$(CONFIG)
	@echo LIBDIR=$(LIBDIR)
	@echo ARCHES=$(ARCHES)
	@echo OUTDIR=$(OUTDIR)


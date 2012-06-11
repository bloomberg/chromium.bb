#
# Copyright (c) 2012 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that be
# found in the LICENSE file.

# A simple GNU Makefile for enuminsts exhaustive instruction enumeration test.
# To build the 64bit test use:
#   shell> make BITS=64  # the default
# To build the 32bit test use:
#   shell> make BITS=32
# note: run "make clean" between 32bit and 64bit builds.

# Defines which version (64 vs 32 bit) you are generating.
BITS=64

# XED - Defines whether the xed decoder should be built into the
# executable.  NOTE: If this is defined as XED=1, you must have the
# corresponding PIN tarfile in the same directory, so that it can
# untar it and add the corresponding library routines.

# NACLV - If defined, the nacl revision number the executable was built using.

# RAGEL - test the ragel decoder

# Defines the pin version to use. Can be overridden on the command line,
# so that other versions can be used.
PINV=2.10-45467

PINBASE=pin-$(PINV)-gcc.3.4.6-ia32_intel64-linux
PINGZ=./$(PINBASE).tar.gz
PINDIR=./$(PINBASE)
XEDKIT=$(PINBASE)/extras/$(if $(filter 32,$(BITS)),xed2-ia32,xed2-intel64)

NACLDIR=../../../../../..
NACLSCONS =$(NACLDIR)/scons-out/opt-linux-x86-$(BITS)
NACLSCONS32 =$(NACLDIR)/scons-out/opt-linux-x86-32

# Define library that must be inserted into NACLLIBS (below) if compiling for 32-bits.
ifeq ($(BITS),32)
LIB32INSERT=$(NACLSCONS32)/lib/libncdis_seg_sfi_verbose_x86_32.a
else
LIB32INSERT=
endif

# NOTE: we need to include some print routines from x86-32 that
# are not in x86-64.
NACLLIBS=\
	$(NACLSCONS)/lib/libncdis_util_x86_$(BITS).a \
	$(NACLSCONS)/lib/libncvalidate_x86_$(BITS).a \
	$(NACLSCONS)/lib/libdfa_decode_x86_32.a \
	$(NACLSCONS)/lib/libdfa_decode_x86_64.a \
	$(NACLSCONS)/lib/libdfa_validate_x86_32.a \
	$(NACLSCONS)/lib/libdfa_validate_x86_64.a \
	$(LIB32INSERT) \
	$(NACLSCONS)/lib/libncval_reg_sfi_x86_$(BITS).a \
	$(NACLSCONS)/lib/libnc_decoder_x86_$(BITS).a \
	$(NACLSCONS)/lib/libnc_opcode_modeling_verbose_x86_$(BITS).a \
	$(NACLSCONS)/lib/libnc_opcode_modeling_x86_$(BITS).a \
	$(NACLSCONS)/lib/libncdis_decode_tables_x86_$(BITS).a \
	$(NACLSCONS)/lib/libncval_seg_sfi_x86_$(BITS).a \
	$(NACLSCONS)/lib/libncdis_seg_sfi_x86_$(BITS).a \
	$(NACLSCONS)/lib/libncval_base_verbose_x86_$(BITS).a \
	$(NACLSCONS)/lib/libncval_base_x86_$(BITS).a \
	$(NACLSCONS)/lib/libplatform.a \
	$(NACLSCONS)/lib/libgio.a \
	$(NACLSCONS)/lib/libutils.a -lpthread


NACLINCLUDE=-I$(NACLDIR)/..
NACLDEFS=-DNACL_TARGET_SUBARCH=$(BITS) -DNACL_BUILD_SUBARCH=$(BITS) \
	 -DNACL_TARGET_ARCH=x86 \
         -DNACL_LINUX -DNACL_TRUSTED_BUT_NOT_TCB -DNACL_PINV='"$(PINV)"'

# Define XED specific command line arguments when including NaCl xed decoder:
#    XEDLIB - libraries containing implementation of xed.
#    XEDINCLUDE - Include directives for xed implementation.
#    XEDDECODER - Object module implementing NaCl xed decoder.
#    XEDDEFS - Defines to add when including NaCl xed decoder.
#    XEDSUFFIX - Suffix to add to executable to state xed is included.
ifdef XED
XEDLIB=$(XEDKIT)/lib/libxed.a
XEDINCLUDE=-I$(XEDKIT)/include
XEDDECODER=xed_tester$(SUFFIX).o
XEDDEFS=-DNACL_XED_DECODER
XEDSUFFIX=-xed
else
XEDLIB=
XEDINCLUDE=
XEDDECODER=
XEDDEFS=
XEDSUFFIX=
endif

# Define NACLV specific changes when NACLV is defined.
ifdef NACLV
NACLVDEFS=-DNACL_REVISION=$(NACLV)
NACLVSUFFIX=-$(NACLV)
else
NACLVDEFS=
NACLVSUFFIX=
endif

ifdef RAGEL
RAGELDECODER=ragel_tester$(SUFFIX).o
RAGELDEFS=-DNACL_RAGEL_DECODER
else
RAGELDECODER=
RAGELDEFS=
endif

SUFFIX=-$(BITS)$(XEDSUFFIX)$(NACLVSUFFIX)
OPTDBG=-g -O1
CC=gcc -m$(BITS)
CXX=g++ -m$(BITS)
CCARGS = -c $(NACLDEFS) $(NACLVDEFS) $(XEDDEFS) $(XEDINCLUDE) $(NACLINCLUDE) $(RAGELDEFS)

enuminsts$(SUFFIX):$(XEDLIB) enuminsts$(SUFFIX).o str_utils$(SUFFIX).o \
		$(RAGELDECODER) \
		$(XEDDECODER) nacl_tester$(SUFFIX).o input_tester$(SUFFIX).o \
		text2hex$(SUFFIX).o
	$(CXX) $(OPTDBG) -o $@ enuminsts$(SUFFIX).o str_utils$(SUFFIX).o \
		$(RAGELDECODER) \
		$(XEDDECODER) nacl_tester$(SUFFIX).o input_tester$(SUFFIX).o \
		text2hex$(SUFFIX).o $(XEDLIB) $(NACLLIBS)

enuminsts$(SUFFIX).o: enuminsts.c
	$(CC) $(OPTDBG) -o enuminsts$(SUFFIX).o $(CCARGS) enuminsts.c

input_tester$(SUFFIX).o: input_tester.c
	$(CC) $(OPTDBG) -o input_tester$(SUFFIX).o $(CCARGS) input_tester.c

nacl_tester$(SUFFIX).o: nacl_tester.c
	$(CC) $(OPTDBG) -o nacl_tester$(SUFFIX).o $(CCARGS) nacl_tester.c

xed_tester$(SUFFIX).o: xed_tester.c
	$(CC) $(OPTDBG) -o xed_tester$(SUFFIX).o $(CCARGS) xed_tester.c

ragel_tester$(SUFFIX).o: ragel_tester.c
	$(CC) $(OPTDBG) -o ragel_tester$(SUFFIX).o $(CCARGS) ragel_tester.c

str_utils$(SUFFIX).o: str_utils.h str_utils.c
	$(CC) $(OPTDBG) -o str_utils$(SUFFIX).o $(CCARGS) str_utils.c

text2hex$(SUFFIX).o: text2hex.c
	$(CC) $(OPTDBG) -o text2hex$(SUFFIX).o $(CCARGS) text2hex.c

$(XEDLIB):$(PINGZ)
	tar xzf $(PINGZ) $(XEDKIT)
	touch $(XEDLIB)

clean:
	-rm -f *~ enuminsts$(SUFFIX) *$(SUFFIX).o
	-rm -rf $(PINBASE)

$(PINGZ):
	@echo "This test requires downloading PIN first."
	@echo "See ./README.txt for details."

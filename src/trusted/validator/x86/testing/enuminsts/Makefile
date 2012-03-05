#
# Copyright (c) 2010 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that be
# found in the LICENSE file.

# A simple GNU Makefile for enuminsts exhaustive instruction enumeration test.
# To build the 64bit test use:
#   shell> make BITS=64  # the default
# To build the 32bit test use:
#   shell> make BITS=32
# note: run "make clean" between 32bit and 64bit builds.
BITS=64

# Defines the pin version to use. Can be overridden on the command line,
# so that other versiosn can be used.
PINV=2.10-45467

PINBASE=pin-$(PINV)-gcc.3.4.6-ia32_intel64-linux
PINGZ=./$(PINBASE).tar.gz
PINDIR=./$(PINBASE)
XEDKIT=$(PINBASE)/extras/$(if $(filter 32,$(BITS)),xed2-ia32,xed2-intel64)
XEDLIB=$(XEDKIT)/lib/libxed.a
XEDINCLUDE=-I$(XEDKIT)/include
NACLDIR=../../../../../..
NACLSCONS =$(NACLDIR)/scons-out/opt-linux-x86-$(BITS)
NACLSCONS32 =$(NACLDIR)/scons-out/opt-linux-x86-32

# NOTE: we need to include some print routines for x86-32 that
# are not in x86-64.
NACLLIBS=\
	$(NACLSCONS)/lib/libncdis_util_x86_$(BITS).a \
	$(NACLSCONS)/lib/libncvalidate_x86_$(BITS).a \
        $(NACLSCONS32)/lib/libncdis_seg_sfi_verbose_x86_32.a \
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
         -DNACL_LINUX -DNACL_TRUSTED_BUT_NOT_TCB
OPTDBG=-O3
CC=gcc -m$(BITS)
CXX=g++ -m$(BITS)

enuminsts-$(BITS):$(XEDLIB) enuminsts-$(BITS).o xed_tester-$(BITS).o
	$(CXX) $(OPTDBG) -o $@ enuminsts-$(BITS).o \
		xed_tester-$(BITS).o $(XEDLIB) $(NACLLIBS)

enuminsts-$(BITS).o: enuminsts.c
	$(CC) $(OPTDBG) -o enuminsts-$(BITS).o -c $(NACLDEFS) $(XEDINCLUDE) \
		$(NACLINCLUDE) enuminsts.c

xed_tester-$(BITS).o: xed_tester.c
	$(CC) $(OPTDBG) -o xed_tester-$(BITS).o -c $(NACLDEFS) $(XEDINCLUDE) \
		$(NACLINCLUDE) xed_tester.c

$(XEDLIB):$(PINGZ)
	tar xzf $(PINGZ) $(XEDKIT)
	touch $(XEDLIB)

clean:
	-rm *~ enuminsts-$(BITS) *-$(BITS).o
	-rm -r $(PINBASE)

$(PINGZ):
	@echo "This test requires downloading PIN first."
	@echo "See ./README.txt for details."

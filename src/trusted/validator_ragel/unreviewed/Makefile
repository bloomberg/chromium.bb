# Copyright (c) 2012 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# A temporary Makefile to build the DFA-based validator, decoder, tests.  This
# will likely go away as soon we integrate with the NaCl build system(s).

OUT = out
OUT_DIRS = $(OUT)/build/objs \
           $(OUT)/tarballs \
           $(OUT)/timestamps \
           $(OUT)/test
OBJD=$(OUT)/build/objs

PYTHON2X=/usr/bin/python2.6
CC = gcc -std=gnu99 -Wdeclaration-after-statement -Wall -pedantic -Wextra \
     -Wno-long-long -Wswitch-enum -Wsign-compare -Wno-variadic-macros -Werror \
     -O3 -finline-limit=10000 -I../../../../.. -DNACL_LINUX
CXX = g++ -std=c++0x -O3 -finline-limit=10000 -DNACL_TRUSTED_BUT_NOT_TCB
RAGEL = ragel
CFLAGS = -g
CXXFLAGS = -g
LDFLAGS = -g
INST_DEFS = general_purpose_instructions.def \
            system_instructions.def \
            x87_instructions.def \
            mmx_instructions.def \
            xmm_instructions.def \
            nops.def

FAST_TMP_FOR_TEST=/dev/shm

# Default rule.
all: outdirs $(OBJD)/decoder_test $(OBJD)/validator_test

# Create all prerequisite directories.
.PHONY: outdirs
outdirs: | $(OUT_DIRS)
$(OUT_DIRS):
	install -m 755 -d $@

# Pattern rules.
$(OBJD)/%.o: $(OBJD)/%.c
	$(CC) $(CFLAGS) -I. -I$(OBJD) -c $< -o $@

$(OBJD)/%.c: %.rl
	$(RAGEL) -G2 -I$(OBJD) $< -o $@

# Decoder, validator, etc.
$(OBJD)/decoder_test: \
    $(OBJD)/decoder_x86_32.o $(OBJD)/decoder_x86_64.o $(OBJD)/decoder_test.o
$(OBJD)/validator_test: \
    $(OBJD)/validator_x86_32.o $(OBJD)/validator_x86_64.o $(OBJD)/validator_test.o

GEN_DFA=$(OBJD)/gen_dfa
$(GEN_DFA): gen_dfa.cc | $(OUT_DIRS)
	$(CXX) $(CXXFLAGS) $< -o $(GEN_DFA)

$(OBJD)/decoder_x86_32.c: $(OBJD)/decoder_x86_32_instruction_consts.c
$(OBJD)/decoder_x86_32.c: $(OBJD)/decoder_x86_32_instruction.rl
$(OBJD)/decoder_x86_32_instruction_consts.c \
  $(OBJD)/decoder_x86_32_instruction.rl: $(GEN_DFA) $(INST_DEFS)
	$(GEN_DFA) -o $(OBJD)/decoder_x86_32_instruction.rl $(INST_DEFS) \
	    -d check_access,opcode,parse_operands_states,mark_data_fields

$(OBJD)/decoder_x86_64.c: $(OBJD)/decoder_x86_64_instruction_consts.c
$(OBJD)/decoder_x86_64.c: $(OBJD)/decoder_x86_64_instruction.rl
$(OBJD)/decoder_x86_64_instruction_consts.c \
  $(OBJD)/decoder_x86_64_instruction.rl: $(GEN_DFA) $(INST_DEFS)
	$(GEN_DFA) -o $(OBJD)/decoder_x86_64_instruction.rl $(INST_DEFS) \
	    -d check_access,opcode,parse_operands_states,mark_data_fields \
	    -m amd64

$(OBJD)/validator_x86_32.c: $(OBJD)/validator_x86_32_instruction.rl
$(OBJD)/validator_x86_32_instruction_consts.c \
  $(OBJD)/validator_x86_32_instruction.rl: $(GEN_DFA) $(INST_DEFS)
	$(GEN_DFA) -o $(OBJD)/validator_x86_32_instruction.rl $(INST_DEFS) \
	  -d check_access,opcode,parse_operands,parse_operands_states \
	  -d instruction_name,mark_data_fields,nacl-forbidden \
	  -d imm_operand_action,rel_operand_action

$(OBJD)/validator_x86_64.c: $(OBJD)/validator_x86_64_instruction_consts.c
$(OBJD)/validator_x86_64.c: $(OBJD)/validator_x86_64_instruction.rl
$(OBJD)/validator_x86_64_instruction_consts.c \
  $(OBJD)/validator_x86_64_instruction.rl: $(GEN_DFA) $(INST_DEFS)
	$(GEN_DFA) -o $(OBJD)/validator_x86_64_instruction.rl $(INST_DEFS) \
	  -d opcode,instruction_name,mark_data_fields,imm_operand_action \
	  -d rel_operand_action,nacl-forbidden,parse_nonwrite_registers \
	  -d parse_x87_operands,parse_mmx_operands,parse_xmm_operands \
	  -d parse_ymm_operands,parse_relative_operands \
	  -d parse_immediate_operands,parse_operands_states \
	  -d parse_operand_positions -m amd64

# Facilities for testing:
#   one_instruction.dot: the description of the DFA that accepts all instruction
#     the decoder is able to decode.
#   decoder_test: the decoder that follows the objdump format
$(OBJD)/one_instruction_x86_32.dot: one_instruction_x86_32.rl \
  $(OBJD)/one_valid_instruction_x86_32_consts.c \
  $(OBJD)/one_valid_instruction_x86_32.rl
	$(RAGEL) -V -I$(OBJD) $< -o $@

$(OBJD)/one_instruction_x86_64.dot: one_instruction_x86_64.rl \
  $(OBJD)/one_valid_instruction_x86_64_consts.c \
  $(OBJD)/one_valid_instruction_x86_64.rl
	$(RAGEL) -V -I$(OBJD) $< -o $@

$(OBJD)/one_valid_instruction_x86_32_consts.c \
    $(OBJD)/one_valid_instruction_x86_32.rl: $(GEN_DFA) $(INST_DEFS)
	$(GEN_DFA) -o $(OBJD)/one_valid_instruction_x86_32.rl $(INST_DEFS) \
	  -d check_access,rex_prefix,vex_prefix,opcode,parse_operands \
	  -d parse_operands_states

$(OBJD)/one_valid_instruction_x86_64_consts.c \
    $(OBJD)/one_valid_instruction_x86_64.rl: $(GEN_DFA) $(INST_DEFS)
	$(GEN_DFA) -o $(OBJD)/one_valid_instruction_x86_64.rl $(INST_DEFS) \
	  -d check_access,rex_prefix,vex_prefix,opcode,parse_operands \
	  -d parse_operands_states -m amd64

$(OBJD)/decoder_test.o: decoder_test.c
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJD)/validator_test.o: validator_test.c
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJD)/validator_x86_64.o: $(OBJD)/validator_x86_64.c
	if [ -e nacl_irt_x86_32.nexe ] && [ -e nacl_irt_x86_64.nexe ]; then \
	  $(CC) $(CFLAGS) -I. -I$(OBJD) -fprofile-generate -c $< -o $@-pf && \
	  $(CC) $(CFLAGS) -I. -I$(OBJD) -fprofile-generate -c \
	    $(OBJD)/validator_x86_32.c -o $(OBJD)/validator_x86_32.o-pf && \
	  $(CC) $(CFLAGS) -fprofile-generate $@-pf \
	    $(OBJD)/validator_x86_32.o-pf validator_test.c \
	    -o $(OBJD)/ncval_train && \
	  $(OBJD)/ncval_train nacl_irt_x86_32.nexe && \
	  $(OBJD)/ncval_train nacl_irt_x86_64.nexe && \
	  rm validator_test.gcda && \
	  $(CC) $(CFLAGS) -I. -I$(OBJD) -fprofile-use -c $< -o $@ && \
	  $(CC) $(CFLAGS) -I. -I$(OBJD) -fprofile-use -c \
	    $(OBJD)/validator_x86_32.c -o $(OBJD)/validator_x86_32.o ; \
	else \
	  $(CC) $(CFLAGS) -I. -I$(OBJD) -c $< -o $@ ; \
	fi

# To test the decoder compare its output with output from objdump.  This
# allows to match instruction opcode, length and operands.
#
# Disassemblers in different versions of binutils produce slightly different
# output.  Do not take binutils as installed on the system, instead download and
# build it.
#
# Original source is located here:
# BINUTILS_URL_BASE = http://ftp.gnu.org/gnu/binutils
BINUTILS_URL_BASE = http://commondatastorage.googleapis.com/nativeclient-mirror/toolchain/binutils
BINUTILS_VER = binutils-2.22
BINUTILS_TARBALL = $(OUT)/tarballs/$(BINUTILS_VER).tar.bz2
BINUTILS_BUILD_DIR = $(OUT)/build/build-$(BINUTILS_VER)
BINUTILS_STAMP = $(OUT)/timestamps/binutils
OBJDUMP = $(BINUTILS_BUILD_DIR)/binutils/objdump
GAS = $(BINUTILS_BUILD_DIR)/gas/as-new

$(BINUTILS_TARBALL): | $(OUT_DIRS)
	rm -f $(BINUTILS_TARBALL)
	cd $(OUT)/tarballs && wget $(BINUTILS_URL_BASE)/$(BINUTILS_VER).tar.bz2

$(BINUTILS_STAMP): $(BINUTILS_TARBALL) | $(OUT_DIRS)
	rm -rf $(OUT)/build/$(BINUTILS_VER)
	cd $(OUT)/build && \
	  tar jxf $(CURDIR)/$(OUT)/tarballs/$(BINUTILS_VER).tar.bz2
	printf -- "--- $(OUT)/build/$(BINUTILS_VER)/opcodes/i386-dis.c\n\
	+++ $(OUT)/build/$(BINUTILS_VER)/opcodes/i386-dis.c\n\
	@@ -2614,6 +2614,12 @@\n\
	   {\n\
	     { \"prefetch\",	{ Mb } },\n\
	     { \"prefetchw\",	{ Mb } },\n\
	+    { Bad_Opcode },\n\
	+    { \"prefetch_modified\",	{ Mb } },\n\
	+    { Bad_Opcode },\n\
	+    { Bad_Opcode },\n\
	+    { Bad_Opcode },\n\
	+    { Bad_Opcode },\n\
	   },\n\
	   /* REG_0F18 */\n\
	   {\n\
	@@ -10490,13 +10496,34 @@\n\
	   {\n\
	     /* RM_0FAE_REG_5 */\n\
	     { \"lfence\",		{ Skip_MODRM } },\n\
	+    { \"lfence\",		{ Skip_MODRM } },\n\
	+    { \"lfence\",		{ Skip_MODRM } },\n\
	+    { \"lfence\",		{ Skip_MODRM } },\n\
	+    { \"lfence\",		{ Skip_MODRM } },\n\
	+    { \"lfence\",		{ Skip_MODRM } },\n\
	+    { \"lfence\",		{ Skip_MODRM } },\n\
	+    { \"lfence\",		{ Skip_MODRM } },\n\
	   },\n\
	   {\n\
	     /* RM_0FAE_REG_6 */\n\
	     { \"mfence\",		{ Skip_MODRM } },\n\
	+    { \"mfence\",		{ Skip_MODRM } },\n\
	+    { \"mfence\",		{ Skip_MODRM } },\n\
	+    { \"mfence\",		{ Skip_MODRM } },\n\
	+    { \"mfence\",		{ Skip_MODRM } },\n\
	+    { \"mfence\",		{ Skip_MODRM } },\n\
	+    { \"mfence\",		{ Skip_MODRM } },\n\
	+    { \"mfence\",		{ Skip_MODRM } },\n\
	   },\n\
	   {\n\
	     /* RM_0FAE_REG_7 */\n\
	     { \"sfence\",		{ Skip_MODRM } },\n\
	+    { \"sfence\",		{ Skip_MODRM } },\n\
	+    { \"sfence\",		{ Skip_MODRM } },\n\
	+    { \"sfence\",		{ Skip_MODRM } },\n\
	+    { \"sfence\",		{ Skip_MODRM } },\n\
	+    { \"sfence\",		{ Skip_MODRM } },\n\
	+    { \"sfence\",		{ Skip_MODRM } },\n\
	+    { \"sfence\",		{ Skip_MODRM } },\n\
	   },\n\
	 };\n\
	@@ -13619,7 +13640,7 @@\n\
	 	}\n\
	       break;\n\
	     case v_mode:\n\
	-      if (sizeflag & DFLAG)\n\
	+      if ((sizeflag & DFLAG) || (rex & REX_W))\n\
	 	op = get32s ();\n\
	       else\n\
	 	op = get16 ();\n\
	--- $(OUT)/build/$(BINUTILS_VER)/opcodes/i386-opc.tbl\n\
	+++ $(OUT)/build/$(BINUTILS_VER)/opcodes/i386-opc.tbl\n\
	@@ -2954,2 +2954,3 @@\n\
	 prefetchw, 1, 0xf0d, 0x1, 2, Cpu3dnow, Modrm|IgnoreSize|No_bSuf|No_wSuf|No_lSuf|No_sSuf|No_qSuf|No_ldSuf, { Byte|Unspecified|BaseIndex|Disp8|Disp16|Disp32|Disp32S }\n\
	+prefetch_modified, 1, 0xf0d, 0x3, 2, Cpu3dnow, Modrm|IgnoreSize|No_bSuf|No_wSuf|No_lSuf|No_sSuf|No_qSuf|No_ldSuf, { Byte|Unspecified|BaseIndex|Disp8|Disp16|Disp32|Disp32S }\n\
	 femms, 0, 0xf0e, None, 2, Cpu3dnow, No_bSuf|No_wSuf|No_lSuf|No_sSuf|No_qSuf|No_ldSuf, { 0 }\n\
	--- $(OUT)/build/$(BINUTILS_VER)/opcodes/i386-tbl.h\n\
	+++ $(OUT)/build/$(BINUTILS_VER)/opcodes/i386-tbl.h\n\
	@@ -35744,6 +35744,16 @@\n\
	     { { { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, \n\
	 	  1, 1, 1, 1, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, \n\
	 	  0, 0, 1, 0, 0, 0 } } } },\n\
	+  { \"prefetch_modified\", 1, 0xf0d, 0x3, 2,\n\
	+    { { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, \n\
	+        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, \n\
	+        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 } },\n\
	+    { 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 1, \n\
	+      1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, \n\
	+      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },\n\
	+    { { { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, \n\
	+	  1, 1, 1, 1, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, \n\
	+	  0, 0, 1, 0, 0, 0 } } } },\n\
	   { \"femms\", 0, 0xf0e, None, 2,\n\
	     { { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, \n\
	         0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, \n" | \
	patch -p0
	rm -rf $(BINUTILS_BUILD_DIR)
	mkdir -p $(BINUTILS_BUILD_DIR)
	cd $(BINUTILS_BUILD_DIR) && \
	  $(CURDIR)/$(OUT)/build/$(BINUTILS_VER)/configure
	$(MAKE) -C $(BINUTILS_BUILD_DIR)
	touch $@

.PHONY: binutils
binutils: $(BINUTILS_STAMP)

# Clean all build artifacts except the binutils' binaries.
.PHONY: clean
clean:
	rm -rf "$(OBJD)" "$(OUT)"/test \
	    "$(FAST_TMP_FOR_TEST)"/_test_dfa_insts*

# Clean everything not including the downloaded tarballs.
.PHONY: clean-all
clean-all: clean
	rm -rf "$(OUT)"/timestamps "$(OUT)"/build

# Clean side effects created while running tests.
.PHONY: clean-tests
clean-tests:
	rm -rf "$(OUT)"/test "$(FAST_TMP_FOR_TEST)"/_test_dfa_insts*
	rm -f dfa_ncval

# The target for all short-running tests.
.PHONY: check
check: check-ncval

# Checks that all byte sequences accepted by the DFA are decoded identically to
# the objdump. A long-running test.
.PHONY: check-decoder
check-decoder: outdirs $(BINUTILS_STAMP) $(OBJD)/one_instruction_x86_32.dot \
    $(OBJD)/one_instruction_x86_64.dot $(OBJD)/decoder_test
	$(PYTHON2X) parse_dfa.py <"$(OBJD)/one_instruction_x86_32.dot" \
	    > "$(OUT)/test/test_dfa_transitions_x86_32.c"
	$(CC) $(CFLAGS) -c test_dfa.c -o "$(OUT)/test/test_dfa.o"
	$(CC) $(CFLAGS) -O0 -I. -c "$(OUT)/test/test_dfa_transitions_x86_32.c" \
	    -o "$(OUT)/test/test_dfa_transitions_x86_32.o"
	$(CC) $(LDFLAGS) "$(OUT)/test/test_dfa.o" \
	    "$(OUT)/test/test_dfa_transitions_x86_32.o" \
	    -o $(OUT)/test/test_dfa_x86_32
	$(PYTHON2X) run_objdump_test.py \
	  --gas="$(GAS) --32" \
	  --objdump="$(OBJDUMP)" \
	  --decoder="$(OBJD)/decoder_test" \
	  --tester=./decoder_test_one_file.sh -- \
	  "$(OUT)/test/test_dfa_x86_32" "$(FAST_TMP_FOR_TEST)"
	$(PYTHON2X) parse_dfa.py <"$(OBJD)/one_instruction_x86_64.dot" \
	    > "$(OUT)/test/test_dfa_transitions_x86_64.c"
	$(CC) $(CFLAGS) -O0 -I. -c "$(OUT)/test/test_dfa_transitions_x86_64.c" \
	    -o "$(OUT)/test/test_dfa_transitions_x86_64.o"
	$(CC) $(LDFLAGS) "$(OUT)/test/test_dfa.o" \
	    "$(OUT)/test/test_dfa_transitions_x86_64.o" \
	    -o $(OUT)/test/test_dfa_x86_64
	$(PYTHON2X) run_objdump_test.py \
	  --gas="$(GAS) --64" \
	  --objdump="$(OBJDUMP)" \
	  --decoder="$(OBJD)/decoder_test" \
	  --tester=./decoder_test_one_file.sh -- \
	  "$(OUT)/test/test_dfa_x86_64" "$(FAST_TMP_FOR_TEST)"

include ncval_tests.txt

# Converts existing validator tests in testdata/64 and produces golden files
# that can be used to compare with the DFA-based validator.
.PHONY: convert-testdata
convert-testdata:
	ln -sfn ../../validator_x86/testdata
	(for tst in $(CONVERTIBLE_TESTS64); do \
	  python parse_hex.py testdata/64/$$tst.rval ; \
	done) || /bin/true
	(for tst in $(CONVERTIBLE_TESTS32); do \
	  python parse_hex.py testdata/32/$$tst.nval ; \
	done) || /bin/true
	mv testdata/64/*.val.ref golden/
	mv testdata/32/*.val.ref golden/32

CHECK_NCVAL_DEPS=outdirs $(BINUTILS_STAMP) $(OBJD)/validator_test \
    $(OBJD)/decoder_test

.PHONY: check-ncval
check-ncval: check-ncval32 check-ncval64

.PHONY: check-ncval32
check-ncval32: $(CHECK_NCVAL_DEPS)
	$(MAKE) check-ncval-impl TEST_ARCH=x86-32

.PHONY: check-ncval64
check-ncval64: $(CHECK_NCVAL_DEPS)
	$(MAKE) check-ncval-impl TEST_ARCH=x86-64

.PHONY: check-ncval-impl
check-ncval-impl:
	ln -sfn ../../validator_x86/testdata
	$(PYTHON2X) run_ncval_tests.py \
	  --decoder="$(OBJD)/decoder_test" \
	  --validator="$(OBJD)/validator_test" \
	  --gas="$(GAS)" \
	  --tmp="$(OUT)/test/" \
	  --arch="$(TEST_ARCH)"

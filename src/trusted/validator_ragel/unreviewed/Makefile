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
INST_DEFS = general-purpose-instructions.def \
            system-instructions.def \
            x87-instructions.def \
            mmx-instructions.def \
            xmm-instructions.def

FAST_TMP_FOR_TEST=/dev/shm

# Default rule.
all: outdirs $(OBJD)/decoder-test $(OBJD)/validator-test

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
$(OBJD)/decoder-test: \
    $(OBJD)/decoder-x86_32.o $(OBJD)/decoder-x86_64.o $(OBJD)/decoder-test.o
$(OBJD)/validator-test: \
    $(OBJD)/validator-x86_32.o $(OBJD)/validator-x86_64.o $(OBJD)/validator-test.o

GEN_DFA=$(OBJD)/gen-dfa
$(GEN_DFA): gen-dfa.cc | $(OUT_DIRS)
	$(CXX) $(CXXFLAGS) $< -o $(GEN_DFA)

$(OBJD)/decoder-x86_32.c: $(OBJD)/decoder-x86_32-instruction-consts.c
$(OBJD)/decoder-x86_32.c: $(OBJD)/decoder-x86_32-instruction.rl
$(OBJD)/decoder-x86_32-instruction-consts.c \
  $(OBJD)/decoder-x86_32-instruction.rl: $(GEN_DFA) $(INST_DEFS)
	$(GEN_DFA) -o $(OBJD)/decoder-x86_32-instruction.rl $(INST_DEFS) \
	    -d check_access,opcode,parse_operands_states,mark_data_fields

$(OBJD)/decoder-x86_64.c: $(OBJD)/decoder-x86_64-instruction-consts.c
$(OBJD)/decoder-x86_64.c: $(OBJD)/decoder-x86_64-instruction.rl
$(OBJD)/decoder-x86_64-instruction-consts.c \
  $(OBJD)/decoder-x86_64-instruction.rl: $(GEN_DFA) $(INST_DEFS)
	$(GEN_DFA) -o $(OBJD)/decoder-x86_64-instruction.rl $(INST_DEFS) \
	    -d check_access,opcode,parse_operands_states,mark_data_fields \
	    -m amd64

$(OBJD)/validator-x86_32.c: $(OBJD)/validator-x86_32-instruction.rl
$(OBJD)/validator-x86_32-instruction-consts.c \
  $(OBJD)/validator-x86_32-instruction.rl: $(GEN_DFA) $(INST_DEFS)
	$(GEN_DFA) -o $(OBJD)/validator-x86_32-instruction.rl $(INST_DEFS) \
	  -d check_access,opcode,parse_operands,parse_operands_states \
	  -d instruction_name,mark_data_fields,nacl-forbidden \
	  -d imm_operand_action,rel_operand_action nops.def

$(OBJD)/validator-x86_64.c: $(OBJD)/validator-x86_64-instruction-consts.c
$(OBJD)/validator-x86_64.c: $(OBJD)/validator-x86_64-instruction.rl
$(OBJD)/validator-x86_64-instruction-consts.c \
  $(OBJD)/validator-x86_64-instruction.rl: $(GEN_DFA) $(INST_DEFS)
	$(GEN_DFA) -o $(OBJD)/validator-x86_64-instruction.rl $(INST_DEFS) \
	  -d opcode,instruction_name,mark_data_fields,rel_operand_action \
	  -d nacl-forbidden,parse_nonwrite_registers,parse_x87_operands \
	  -d parse_mmx_operands,parse_xmm_operands,parse_ymm_operands \
	  -d parse_relative_operands,parse_immediate_operands \
	  -d parse_operands_states,parse_operand_positions nops.def -m amd64

# Facilities for testing:
#   one-instruction.dot: the description of the DFA that accepts all instruction
#     the decoder is able to decode.
#   decoder-test: the decoder that follows the objdump format
$(OBJD)/one-instruction-x86_32.dot: one-instruction-x86_32.rl \
  $(OBJD)/one-valid-instruction-x86_32-consts.c \
  $(OBJD)/one-valid-instruction-x86_32.rl
	$(RAGEL) -V -I$(OBJD) $< -o $@

$(OBJD)/one-instruction-x86_64.dot: one-instruction-x86_64.rl \
  $(OBJD)/one-valid-instruction-x86_64-consts.c \
  $(OBJD)/one-valid-instruction-x86_64.rl
	$(RAGEL) -V -I$(OBJD) $< -o $@

$(OBJD)/one-valid-instruction-x86_32-consts.c \
    $(OBJD)/one-valid-instruction-x86_32.rl: $(GEN_DFA) $(INST_DEFS)
	$(GEN_DFA) -o $(OBJD)/one-valid-instruction-x86_32.rl $(INST_DEFS) \
	  -d check_access,rex_prefix,vex_prefix,opcode,parse_operands \
	  -d parse_operands_states

$(OBJD)/one-valid-instruction-x86_64-consts.c \
    $(OBJD)/one-valid-instruction-x86_64.rl: $(GEN_DFA) $(INST_DEFS)
	$(GEN_DFA) -o $(OBJD)/one-valid-instruction-x86_64.rl $(INST_DEFS) \
	  -d check_access,rex_prefix,vex_prefix,opcode,parse_operands \
	  -d parse_operands_states -m amd64

$(OBJD)/decoder-test.o: decoder-test.c
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJD)/validator-test.o: validator-test.c
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJD)/validator-x86_64.o: $(OBJD)/validator-x86_64.c
	if [ -e nacl_irt_x86_32.nexe ] && [ -e nacl_irt_x86_64.nexe ]; then \
	  $(CC) $(CFLAGS) -I. -I$(OBJD) -fprofile-generate -c $< -o $@-pf && \
	  $(CC) $(CFLAGS) -I. -I$(OBJD) -fprofile-generate -c \
	    $(OBJD)/validator-x86_32.c -o $(OBJD)/validator-x86_32.o-pf && \
	  $(CC) $(CFLAGS) -fprofile-generate $@-pf \
	    $(OBJD)/validator-x86_32.o-pf validator-test.c \
	    -o $(OBJD)/ncval_train && \
	  $(OBJD)/ncval_train nacl_irt_x86_32.nexe && \
	  $(OBJD)/ncval_train nacl_irt_x86_64.nexe && \
	  rm validator-test.gcda && \
	  $(CC) $(CFLAGS) -I. -I$(OBJD) -fprofile-use -c $< -o $@ && \
	  $(CC) $(CFLAGS) -I. -I$(OBJD) -fprofile-use -c \
	    $(OBJD)/validator-x86_32.c -o $(OBJD)/validator-x86_32.o ; \
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
check: check-irt check-as-alt-validator check-ncval

# Checks that the IRT is not rejected by the validator.
.PHONY: check-irt
check-irt: outdirs $(OBJD)/validator-test
	$(OBJD)/validator-test nacl_irt_x86_64.nexe

.PHONY: check-as-alt-validator
check-as-alt-validator: $(OBJD)/validator-test
	ln -sfn $(OBJD)/validator-test dfa_ncval
	$(PYTHON2X) validator_test.py
	rm -f dfa_ncval

# Checks that all byte sequences accepted by the DFA are decoded identically to
# the objdump. A long-running test.
.PHONY: check-decoder
check-decoder: outdirs $(BINUTILS_STAMP) $(OBJD)/one-instruction-x86_32.dot \
    $(OBJD)/one-instruction-x86_64.dot $(OBJD)/decoder-test
	$(PYTHON2X) parse_dfa.py <"$(OBJD)/one-instruction-x86_32.dot" \
	    > "$(OUT)/test/test_dfa_transitions-x86_32.c"
	$(CC) $(CFLAGS) -c test_dfa.c -o "$(OUT)/test/test_dfa.o"
	$(CC) $(CFLAGS) -O0 -I. -c "$(OUT)/test/test_dfa_transitions-x86_32.c" \
	    -o "$(OUT)/test/test_dfa_transitions-x86_32.o"
	$(CC) $(LDFLAGS) "$(OUT)/test/test_dfa.o" \
	    "$(OUT)/test/test_dfa_transitions-x86_32.o" \
	    -o $(OUT)/test/test_dfa-x86_32
	$(PYTHON2X) run_objdump_test.py \
	  --gas="$(GAS) --32" \
	  --objdump="$(OBJDUMP)" \
	  --decoder="$(OBJD)/decoder-test" \
	  --tester=./decoder_test_one_file.sh \
	  --nthreads=`cat /proc/cpuinfo | grep processor | wc -l` -- \
	  "$(OUT)/test/test_dfa-x86_32" "$(FAST_TMP_FOR_TEST)"
	$(PYTHON2X) parse_dfa.py <"$(OBJD)/one-instruction-x86_64.dot" \
	    > "$(OUT)/test/test_dfa_transitions-x86_64.c"
	$(CC) $(CFLAGS) -O0 -I. -c "$(OUT)/test/test_dfa_transitions-x86_64.c" \
	    -o "$(OUT)/test/test_dfa_transitions-x86_64.o"
	$(CC) $(LDFLAGS) "$(OUT)/test/test_dfa.o" \
	    "$(OUT)/test/test_dfa_transitions-x86_64.o" \
	    -o $(OUT)/test/test_dfa-x86_64
	$(PYTHON2X) run_objdump_test.py \
	  --gas="$(GAS) --64" \
	  --objdump="$(OBJDUMP)" \
	  --decoder="$(OBJD)/decoder-test" \
	  --tester=./decoder_test_one_file.sh \
	  --nthreads=`cat /proc/cpuinfo | grep processor | wc -l` -- \
	  "$(OUT)/test/test_dfa-x86_64" "$(FAST_TMP_FOR_TEST)"

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

CHECK_NCVAL_DEPS=outdirs $(BINUTILS_STAMP) $(OBJD)/validator-test \
    $(OBJD)/decoder-test

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
	  --decoder="$(OBJD)/decoder-test" \
	  --validator="$(OBJD)/validator-test" \
	  --gas="$(GAS)" \
	  --tmp="$(OUT)/test/" \
	  --arch="$(TEST_ARCH)"

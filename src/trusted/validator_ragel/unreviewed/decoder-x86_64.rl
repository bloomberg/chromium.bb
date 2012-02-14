/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <assert.h>
#include <elf.h>
#include <inttypes.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "decoder.h"

#undef TRUE
#define TRUE    1

#undef FALSE
#define FALSE   0

#include "decoder-x86_64-instruction-consts.c"

%%{
  machine x86_64_decoder;
  alphtype unsigned char;

  include decode_x86_64 "decoder-x86_64-instruction.rl";

  main := (one_instruction
    >{
	begin = p;
	disp_type = DISPNONE;
	imm_operand = IMMNONE;
	imm2_operand = IMMNONE;
	rex_prefix = FALSE;
	data16_prefix = FALSE;
	lock_prefix = FALSE;
	repnz_prefix = FALSE;
	repz_prefix = FALSE;
	branch_not_taken = FALSE;
	branch_taken = FALSE;
	vex_prefix2 = 0xe0;
	vex_prefix3 = 0x00;
    }
    @{
	switch (disp_type) {
	  case DISPNONE: instruction.rm.offset = 0; break;
	  case DISP8: instruction.rm.offset = (uint64_t) *disp; break;
	  case DISP16: instruction.rm.offset =
	    (uint64_t) (disp[0] + 256U * disp[1]);
	    break;
	  case DISP32: instruction.rm.offset = (uint64_t)
	    (disp[0] + 256U * (disp[1] + 256U * (disp[2] + 256U * (disp[3]))));
	    break;
	  case DISP64: instruction.rm.offset = (uint64_t)
	    (*disp + 256ULL * (disp[1] + 256ULL * (disp[2] + 256ULL * (disp[3] +
	    256ULL * (disp[4] + 256ULL * (disp[5] + 256ULL * (disp[6] + 256ULL *
								 disp[7])))))));
	    break;
	}
	switch (imm_operand) {
	  case IMMNONE: instruction.imm[0] = 0; break;
	  case IMM2: instruction.imm[0] = imm[0] & 0x03; break;
	  case IMM8: instruction.imm[0] = imm[0]; break;
	  case IMM16: instruction.imm[0] = (uint64_t) (*imm + 256U * (imm[1]));
	    break;
	  case IMM32: instruction.imm[0] = (uint64_t)
	    (imm[0] + 256U * (imm[1] + 256U * (imm[2] + 256U * (imm[3]))));
	    break;
	  case IMM64: instruction.imm[0] = (uint64_t)
	    (imm[0] + 256LL * (imm[1] + 256ULL * (imm[2] + 256ULL * (imm[3] +
	    256ULL * (imm[4] + 256ULL * (imm[5] + 256ULL * (imm[6] + 256ULL *
								  imm[7])))))));
	    break;
	}
	switch (imm2_operand) {
	  case IMMNONE: instruction.imm[1] = 0; break;
	  case IMM2: instruction.imm[1] = imm2[0] & 0x03; break;
	  case IMM8: instruction.imm[1] = imm2[0]; break;
	  case IMM16: instruction.imm[1] = (uint64_t)
	    (imm2[0] + 256U * (imm2[1]));
	    break;
	  case IMM32: instruction.imm[1] = (uint64_t)
	    (imm2[0] + 256U * (imm2[1] + 256U * (imm2[2] + 256U * (imm2[3]))));
	    break;
	  case IMM64: instruction.imm[1] = (uint64_t)
	    (*imm2 + 256ULL * (imm2[1] + 256ULL * (imm2[2] + 256ULL * (imm2[3] +
	    256ULL * (imm2[4] + 256ULL * (imm2[5] + 256ULL * (imm2[6] + 256ULL *
								 imm2[7])))))));
	    break;
	}
	process_instruction(begin, p+1, &instruction, userdata);
    })*
    $!{ process_error(p, userdata);
	result = 1;
	goto error_detected;
    };

}%%

%% write data;

#define base instruction.rm.base
#define index instruction.rm.index
#define scale instruction.rm.scale
#define rex_prefix instruction.prefix.rex
#define data16_prefix instruction.prefix.data16
#define lock_prefix instruction.prefix.lock
#define repz_prefix instruction.prefix.repz
#define repnz_prefix instruction.prefix.repnz
#define branch_not_taken instruction.prefix.branch_not_taken
#define branch_taken instruction.prefix.branch_taken
#define operand0_type instruction.operands[0].type
#define operand1_type instruction.operands[1].type
#define operand2_type instruction.operands[2].type
#define operand3_type instruction.operands[3].type
#define operand4_type instruction.operands[4].type
#define operand0 instruction.operands[0].name
#define operand1 instruction.operands[1].name
#define operand2 instruction.operands[2].name
#define operand3 instruction.operands[3].name
#define operand4 instruction.operands[4].name
#define operands_count instruction.operands_count
#define instruction_name instruction.name

enum {
  REX_B = 1,
  REX_X = 2,
  REX_R = 4,
  REX_W = 8
};

enum disp_mode {
  DISPNONE,
  DISP8,
  DISP16,
  DISP32,
  DISP64,
};

enum imm_mode {
  IMMNONE,
  IMM2,
  IMM8,
  IMM16,
  IMM32,
  IMM64
};

int DecodeChunkAMD64(const uint8_t *data, size_t size,
		     process_instruction_func process_instruction,
		     process_error_func process_error, void *userdata) {
  const uint8_t *p = data;
  const uint8_t *pe = data + size;
  const uint8_t *eof = pe;
  const uint8_t *disp = NULL;
  const uint8_t *imm = NULL;
  const uint8_t *imm2 = NULL;
  const uint8_t *begin;
  uint8_t vex_prefix2, vex_prefix3;
  enum disp_mode disp_type;
  enum imm_mode imm_operand;
  enum imm_mode imm2_operand;
  struct instruction instruction;
  int result = 0;

  int cs;

  %% write init;
  %% write exec;

error_detected:
  return result;
}

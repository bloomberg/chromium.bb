/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * Full-blown decoder for ia32 case.  Can be used to decode instruction sequence
 * and process it, but right now is only used in tests.
 *
 * The code is in [hand-written] “parse_instruction.rl” and in [auto-generated]
 * “decoder_x86_32_instruction.rl” file.  This file only includes tiny amount
 * of the glue code.
 */

#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "native_client/src/shared/utils/types.h"
#include "native_client/src/trusted/validator_ragel/unreviewed/decoder_internal.h"

/*
 * These prefixes are not useful in IA32 mode, but they will “cleaned up” by
 * decoder's cleanup procedure anyway.  Do nothing when that happens.
 */
#define SET_REX_PREFIX(P)
#define SET_VEX_PREFIX2(P)
#define CLEAR_SPURIOUS_REX_B()
#define SET_SPURIOUS_REX_B()
#define CLEAR_SPURIOUS_REX_X()
#define SET_SPURIOUS_REX_X()
#define CLEAR_SPURIOUS_REX_R()
#define SET_SPURIOUS_REX_R()
#define CLEAR_SPURIOUS_REX_W()
#define SET_SPURIOUS_REX_W()

%%{
  machine x86_32_decoder;
  alphtype unsigned char;
  variable p current_position;
  variable pe end_of_data;
  variable eof end_of_data;
  variable cs current_state;

  include byte_machine "byte_machines.rl";

  include prefix_actions
    "native_client/src/trusted/validator_ragel/unreviewed/parse_instruction.rl";
  include prefixes_parsing
    "native_client/src/trusted/validator_ragel/unreviewed/parse_instruction.rl";
  include vex_actions_ia32
    "native_client/src/trusted/validator_ragel/unreviewed/parse_instruction.rl";
  include vex_parsing_ia32
    "native_client/src/trusted/validator_ragel/unreviewed/parse_instruction.rl";
  include att_suffix_actions
    "native_client/src/trusted/validator_ragel/unreviewed/parse_instruction.rl";
  include set_spurious_prefixes
    "native_client/src/trusted/validator_ragel/unreviewed/parse_instruction.rl";
  include displacement_fields_actions
    "native_client/src/trusted/validator_ragel/unreviewed/parse_instruction.rl";
  include displacement_fields_parsing
    "native_client/src/trusted/validator_ragel/unreviewed/parse_instruction.rl";
  include modrm_actions_ia32
    "native_client/src/trusted/validator_ragel/unreviewed/parse_instruction.rl";
  include modrm_parsing_ia32
    "native_client/src/trusted/validator_ragel/unreviewed/parse_instruction.rl";
  include operand_actions_ia32
    "native_client/src/trusted/validator_ragel/unreviewed/parse_instruction.rl";
  include immediate_fields_actions
    "native_client/src/trusted/validator_ragel/unreviewed/parse_instruction.rl";
  include immediate_fields_parsing_ia32
    "native_client/src/trusted/validator_ragel/unreviewed/parse_instruction.rl";
  include relative_fields_actions
    "native_client/src/trusted/validator_ragel/unreviewed/parse_instruction.rl";
  include relative_fields_parsing
    "native_client/src/trusted/validator_ragel/unreviewed/parse_instruction.rl";
  include cpuid_actions
    "native_client/src/trusted/validator_ragel/unreviewed/parse_instruction.rl";

  include decode_x86_32 "decoder_x86_32_instruction.rl";

  include decoder
    "native_client/src/trusted/validator_ragel/unreviewed/parse_instruction.rl";

  main := decoder;
}%%

%% write data;

int DecodeChunkIA32(const uint8_t *data, size_t size,
                    ProcessInstructionFunc process_instruction,
                    ProcessDecodingErrorFunc process_error, void *userdata) {
  const uint8_t *current_position = data;
  const uint8_t *end_of_data = data + size;
  const uint8_t *instruction_start = current_position;
  uint8_t vex_prefix3 = 0x00;
  enum ImmediateMode imm_operand = IMMNONE;
  enum ImmediateMode imm2_operand = IMMNONE;
  struct Instruction instruction;
  int result = TRUE;

  int current_state;

  /* Not used in ia32_mode.  */
  instruction.prefix.rex = 0;

  SET_DISP_TYPE(DISPNONE);
  SET_IMM_TYPE(IMMNONE);
  SET_IMM2_TYPE(IMMNONE);
  SET_DATA16_PREFIX(FALSE);
  SET_LOCK_PREFIX(FALSE);
  SET_REPNZ_PREFIX(FALSE);
  SET_REPZ_PREFIX(FALSE);
  SET_BRANCH_NOT_TAKEN(FALSE);
  SET_BRANCH_TAKEN(FALSE);
  SET_ATT_INSTRUCTION_SUFFIX(NULL);
  instruction.prefix.data16_spurious = FALSE;
  instruction.prefix.rex_b_spurious = FALSE;
  instruction.prefix.rex_x_spurious = FALSE;
  instruction.prefix.rex_r_spurious = FALSE;
  instruction.prefix.rex_w_spurious = FALSE;

  %% write init;
  %% write exec;

error_detected:
  return result;
}

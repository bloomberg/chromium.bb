/*
 * Copyright 2009 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

/*
 * Defines an instruction (decoder), based on the current location of
 * the instruction iterator. The instruction decoder takes a list
 * of candidate opcode (instruction) patterns, and searches for the
 * first candidate that matches the requirements of the opcode pattern.
 */

#include <stdio.h>
#include <assert.h>

#include "native_client/src/trusted/validator_x86/nc_inst_state.h"

#include "native_client/src/shared/platform/nacl_log.h"
#include "native_client/src/trusted/validator_x86/nc_inst_iter.h"
#include "native_client/src/trusted/validator_x86/nc_inst_state_internal.h"
#include "native_client/src/trusted/validator_x86/nc_inst_trans.h"
#include "native_client/src/trusted/validator_x86/nc_segment.h"
#include "native_client/src/trusted/validator_x86/ncop_exps.h"
#include "native_client/src/trusted/validator_x86/ncopcode_desc.h"
#ifdef _WIN64
#include "gen/native_client/src/trusted/validator_x86/nc_opcode_table64.h"
#else
#include "gen/native_client/src/trusted/validator_x86/nc_opcode_table.h"
#endif

/* To turn on debugging of instruction decoding, change value of
 * DEBUGGING to 1.
 */
#define DEBUGGING 0

#include "native_client/src/shared/utils/debugging.h"

/* Given the current location of the instruction iterator, initialize
 * the given state (to match).
 */
static void NaClInstStateInit(NaClInstIter* iter, NaClInstState* state) {
  NaClMemorySize limit;
  state->mpc = iter->segment->mbase + iter->index;
  state->vpc = iter->segment->vbase + iter->index;
  state->length = 0;
  limit = iter->segment->size - iter->index;
  if (limit > NACL_MAX_BYTES_PER_X86_INSTRUCTION) {
    limit = NACL_MAX_BYTES_PER_X86_INSTRUCTION;
  }
  state->length_limit = (uint8_t) limit;
  DEBUG(NaClLog(LOG_INFO,
                "length limit = %"NACL_PRIu8"\n", state->length_limit));
  state->num_prefix_bytes = 0;
  state->rexprefix = 0;
  state->num_rex_prefixes = 0;
  state->prefix_mask = 0;
  state->inst = NULL;
  state->nodes.is_defined = FALSE;
  state->nodes.number_expr_nodes = 0;
}

/* Computes the number of bytes defined for operands of the matched
 * instruction of the given state.
 */
static int NaClExtractOpSize(NaClInstState* state) {
  if (state->inst->flags & NACL_IFLAG(OperandSize_b)) {
    return 1;
  }
  if (NACL_TARGET_SUBARCH == 64) {
    if ((state->rexprefix && state->rexprefix & 0x8) ||
        (state->inst->flags & NACL_IFLAG(OperandSizeForce64))) {
      return 8;
    }
  }
  if ((state->prefix_mask & kPrefixDATA16) &&
      (NACL_EMPTY_IFLAGS ==
       (state->inst->flags & NACL_IFLAG(SizeIgnoresData16)))) {
    return 2;
  }
  if ((NACL_TARGET_SUBARCH == 64) &&
      (state->inst->flags & NACL_IFLAG(OperandSizeDefaultIs64))) {
    return 8;
  }
  return 4;
}

/* Computes the number of bits defined for addresses of the matched
 * instruction of the given state.
 */
static int NaClExtractAddressSize(NaClInstState* state) {
  if (NACL_TARGET_SUBARCH == 64) {
    return (state->prefix_mask & kPrefixADDR16) ? 32 : 64;
  } else {
    return (state->prefix_mask & kPrefixADDR16) ? 16 : 32;
  }
}

/* Manual implies only 4 bytes is allowed, but I have found up to 6.
 * Why don't we allow any number, so long as (1) There is room for
 * at least one opcode byte, and (2) we don't exceed the max bytes.
 */
static const int kNaClMaximumPrefixBytes =
    NACL_MAX_BYTES_PER_X86_INSTRUCTION - 1;

/* Match any prefix bytes that can be associated with the instruction
 * currently being matched.
 */
static Bool NaClConsumePrefixBytes(NaClInstState* state) {
  uint8_t next_byte;
  int i;
  uint32_t prefix_form;
  int rex_index = -1;
  for (i = 0; i < kNaClMaximumPrefixBytes; ++i) {
    /* Quit early if no more bytes in segment. */
    if (state->length >= state->length_limit) break;

    /* Look up the corresponding prefix bit associated
     * with the next byte in the segment, and record it.
     */
    next_byte = state->mpc[state->length];
    prefix_form = kNaClPrefixTable[next_byte];
    if (prefix_form == 0) break;
    DEBUG(NaClLog(LOG_INFO,
                  "Consume prefix[%d]: %02"NACL_PRIx8" => %"NACL_PRIx32"\n",
                  i, next_byte, prefix_form));
    state->prefix_mask |= prefix_form;
    ++state->num_prefix_bytes;
    ++state->length;
    DEBUG(NaClLog(LOG_INFO,
                  "  prefix mask: %08"NACL_PRIx32"\n", state->prefix_mask));

    /* If the prefix byte is a REX prefix, record its value, since
     * bits 5-8 of this prefix bit may be needed later.
     */
    if (NACL_TARGET_SUBARCH == 64) {
      if (prefix_form == kPrefixREX) {
        state->rexprefix = next_byte;
        DEBUG(NaClLog(LOG_INFO,
                      "  rexprefix = %02"NACL_PRIx8"\n", state->rexprefix));
        ++state->num_rex_prefixes;
        rex_index = i;
      }
    }
  }
  if (NACL_TARGET_SUBARCH == 64) {
    /* REX prefix must be last, unless FO exists. If FO
     * exists, it must be after REX (Intel Manual).
     *
     * NOTE: (karl) It appears that this constraint is violated
     * with compiled code of /bin/ld_static. According to AMD,
     * the rex prefix must be last. Changing code to allow REX
     * prefix to occur anywhere.
     */
    if (rex_index >= 0) {
      return (rex_index + 1) == state->num_prefix_bytes;
    }
  }
  return TRUE;
}

/* Structure holding the results of consuming the opcode bytes of the
 * instruction.
 */
typedef struct {
  /* The (last) byte of the matched opcode. */
  uint8_t opcode_byte;
  /* The most specific prefix that the opcode bytes can match
   * (or OpcodePrefixEnumSize if no such patterns exist).
   */
  NaClInstPrefix matched_prefix;
  /* The number of bytes to subtract from the instruction length,
   * the next time GetNextNaClInstCandidates is called.
   */
  uint8_t next_length_adjustment;
} NaClInstPrefixDescriptor;

/* Assuming we have matched the byte sequence OF 38, consume the corresponding
 * following (instruction) opcode byte, returning the most specific prefix the
 * patterns can match (or NaClInstPrefixEnumSize if no such patterns exist);
 */
static void NaClConsume0F38XXNaClInstBytes(NaClInstState* state,
                                           NaClInstPrefixDescriptor* desc) {
  /* Fail if there are no more bytes. Otherwise, read the next
   * byte.
   */
  if (state->length >= state->length_limit) {
    desc->matched_prefix = NaClInstPrefixEnumSize;
    return;
  }

  desc->opcode_byte = state->mpc[state->length++];
  if (state->prefix_mask & kPrefixREPNE) {
    desc->matched_prefix = PrefixF20F38;
  }
  else if (state->prefix_mask & kPrefixDATA16) {
    desc->matched_prefix = Prefix660F38;
  } else if ((state->prefix_mask & ~kPrefixREX) == 0) {
    desc->matched_prefix = Prefix0F38;
  } else {
    /* Other prefixes like F3 cause an undefined instruction error. */
    desc->matched_prefix = NaClInstPrefixEnumSize;
  }
}

/* Assuming we have matched the byte sequence OF 3A, consume the corresponding
 * following (instruction) opcode byte, returning the most specific prefix the
 * patterns can match (or NaClInstPrefixEnumSize if no such patterns exist).
 */
static void NaClConsume0F3AXXNaClInstBytes(NaClInstState* state,
                                           NaClInstPrefixDescriptor* desc) {
  /* Fail if there are no more bytes. Otherwise, read the next
   * byte and choose appropriate prefix.
   */
  if (state->length >= state->length_limit) {
    desc->matched_prefix = NaClInstPrefixEnumSize;
    return;
  }

  desc->opcode_byte = state->mpc[state->length++];
  if (state->prefix_mask & kPrefixDATA16) {
    desc->matched_prefix = Prefix660F3A;
  } else if ((state->prefix_mask & ~kPrefixREX) == 0) {
    desc->matched_prefix = Prefix0F3A;
  } else {
    /* Other prefixes like F3 cause an undefined instruction error. */
    desc->matched_prefix = NaClInstPrefixEnumSize;
  }
}

/* Assuming we have matched byte OF, consume the corresponding
 * following (instruction) opcode byte, returning the most specific
 * prefix the patterns can match (or NaClInstPrefixEnumSize if no such
 * patterns exist).
 */
static void NaClConsume0FXXNaClInstBytes(NaClInstState* state,
                                         NaClInstPrefixDescriptor* desc) {
  if (state->prefix_mask & kPrefixREPNE) {
    desc->matched_prefix = PrefixF20F;
  } else if (state->prefix_mask & kPrefixREP) {
    desc->matched_prefix = PrefixF30F;
  } else if (state->prefix_mask & kPrefixDATA16) {
    desc->matched_prefix = Prefix660F;
  } else {
    desc->matched_prefix = Prefix0F;
  }
}

/* Consume one of the x87 instructions that begin with D8-Df, and
 * match the most specific prefix pattern the opcode bytes can match.
 */
static void NaClConsumeX87NaClInstBytes(NaClInstState* state,
                                        NaClInstPrefixDescriptor* desc) {
  if (state->length < state->length_limit) {
    /* Can be two byte opcode. */
    desc->matched_prefix = PrefixD8 +
        (((unsigned) desc->opcode_byte) - 0xD8);
    desc->opcode_byte = state->mpc[state->length++];
    return;
  }

  /* If reached, can only be single byte opcode, match as such. */
  desc->matched_prefix = NoPrefix;
}

/* Consume the opcode bytes, and return the most specific prefix pattern
 * the opcode bytes can match (or NaClInstPrefixEnumSize if no such pattern
 * exists).
 */
static void NaClConsumeInstBytes(NaClInstState* state,
                                 NaClInstPrefixDescriptor* desc) {

  /* Initialize descriptor to the fail state. */
  desc->opcode_byte = 0x0;
  desc->matched_prefix = NaClInstPrefixEnumSize;
  desc->next_length_adjustment = 0;

  /* Be sure that we don't exceed the segment length. */
  if (state->length >= state->length_limit) return;

  desc->opcode_byte = state->mpc[state->length++];
  switch (desc->opcode_byte) {
    case 0x0F:
      if (state->length >= state->length_limit) return;
      desc->opcode_byte = state->mpc[state->length++];
      switch (desc->opcode_byte) {
        case 0x38:
          NaClConsume0F38XXNaClInstBytes(state, desc);
          break;
        case 0x3a:
          NaClConsume0F3AXXNaClInstBytes(state, desc);
          break;
        default:
          NaClConsume0FXXNaClInstBytes(state, desc);
          break;
      }
      break;
    case 0xD8:
    case 0xD9:
    case 0xDA:
    case 0xDB:
    case 0xDC:
    case 0xDD:
    case 0xDE:
    case 0xDF:
      NaClConsumeX87NaClInstBytes(state, desc);
      break;
    default:
      desc->matched_prefix = NoPrefix;
      break;
  }
  DEBUG(NaClLog(LOG_INFO,
                "matched prefix = %s\n",
                NaClInstPrefixName(desc->matched_prefix)));
}

/* Compute the operand and address sizes for the instruction. Then, verify
 * that the opcode (instruction) pattern allows for such sizes. Aborts
 * the pattern match if any problems.
 */
static Bool NaClConsumeAndCheckOperandSize(NaClInstState* state) {
  state->operand_size = NaClExtractOpSize(state);
  DEBUG(NaClLog(LOG_INFO,
                "operand size = %"NACL_PRIu8"\n", state->operand_size));
  if (state->inst->flags &
      (NACL_IFLAG(OperandSize_w) | NACL_IFLAG(OperandSize_v) |
       NACL_IFLAG(OperandSize_o))) {
    NaClIFlags good = 1;
    switch (state->operand_size) {
      case 2:
        good = (state->inst->flags & NACL_IFLAG(OperandSize_w));
        break;
      case 4:
        good = (state->inst->flags & NACL_IFLAG(OperandSize_v));
        break;
      case 8:
        good = (state->inst->flags & NACL_IFLAG(OperandSize_o));
        break;
      default:
        good = 0;
        break;
    }
    if (!good) {
      /* The flags associated with the opcode (instruction) don't
       * allow the computed sizes, abort the  match of the instruction.
       */
      DEBUG(NaClLog(LOG_INFO,
                    "Operand size %"NACL_PRIu8
                    " doesn't match flag requirement!\n",
                    state->operand_size));
      return FALSE;
    }
  }
  return TRUE;
}

static Bool NaClConsumeAndCheckAddressSize(NaClInstState* state) {
  state->address_size = NaClExtractAddressSize(state);
  DEBUG(NaClLog(LOG_INFO,
                "Address size = %"NACL_PRIu8"\n", state->address_size));
  if (state->inst->flags &
      (NACL_IFLAG(AddressSize_w) | NACL_IFLAG(AddressSize_v) |
       NACL_IFLAG(AddressSize_o))) {
    NaClIFlags good = 1;
    switch (state->address_size) {
    case 16:
      good = (state->inst->flags & NACL_IFLAG(AddressSize_w));
      break;
    case 32:
      good = (state->inst->flags & NACL_IFLAG(AddressSize_v));
      break;
    case 64:
      good = (state->inst->flags & NACL_IFLAG(AddressSize_o));
      break;
    default:
      good = 0;
      break;
    }
    if (!good) {
      /* The flags associated with the opcode (instruction) don't
       * allow the computed sizes, abort the  match of the instruction.
       */
      DEBUG(NaClLog(LOG_INFO,
                    "Address size %"NACL_PRIu8
                    " doesn't match flag requirement!\n",
                    state->address_size));
      return FALSE;
    }
  }
  return TRUE;
}

/* Returns true if the instruction requires a ModRm bytes. */
static Bool NaClInstRequiresModRm(NaClInstState* state) {
  return
      (NACL_EMPTY_IFLAGS !=
       (state->inst->flags & NACL_IFLAG(OpcodeUsesModRm)));
}

/* Consume the Mod/Rm byte of the instruction, if applicable.
 * Aborts the pattern match if any problems.
 */
static Bool NaClConsumeModRm(NaClInstState* state) {
  /* First check if the opcode (instruction) pattern specifies that
   * a Mod/Rm byte is needed, and that reading it will not walk
   * past the end of the code segment.
   */
  if (NaClInstRequiresModRm(state)) {
    uint8_t byte;
    /* Has modrm byte. */
    if (state->length >= state->length_limit) {
      DEBUG(NaClLog(LOG_INFO, "Can't read mod/rm, no more bytes!\n"));
      return FALSE;
    }
    byte = state->mpc[state->length];
    /* Note: Some instructions only allow values where the ModRm mod field
     * is 0x3. Others only allow values where the ModRm mod field isn't 0x3.
     */
    if (modrm_mod(byte) == 0x3) {
      if (state->inst->flags & NACL_IFLAG(ModRmModIsnt0x3)) {
        DEBUG(NaClLog(LOG_INFO, "Can't match, modrm mod field is 0x3\n"));
        return FALSE;
      }
    } else {
      if (state->inst->flags & NACL_IFLAG(ModRmModIs0x3)) {
        DEBUG(NaClLog(LOG_INFO, "Can't match, modrm mod field not 0x3\n"));
        return FALSE;
      }
    }
    if ((state->inst->flags & NACL_IFLAG(ModRmRegSOperand)) &&
        (modrm_reg(byte) > 5)) {
      DEBUG(NaClLog(LOG_INFO,
                    "Can't match, modrm reg field doesn't index segment\n"));
      return FALSE;
    }
    state->modrm = byte;
    state->length++;
    state->num_disp_bytes = 0;
    state->first_disp_byte = 0;
    state->sib = 0;
    state->has_sib = FALSE;
    DEBUG(NaClLog(LOG_INFO, "consume modrm = %02"NACL_PRIx8"\n", state->modrm));

    /* Consume the remaining opcode value in the mod/rm byte
     * if applicable.
     */
    if (state->inst->flags & NACL_IFLAG(OpcodeInModRm)) {
      NaClInst* inst = state->inst;
      if (modrm_opcode(state->modrm) != inst->opcode[inst->num_opcode_bytes]) {
        DEBUG(
            NaClLog(LOG_INFO,
                    "Discarding, opcode in mrm byte (%02"NACL_PRIx8") "
                    "does not match\n",
                    modrm_opcode(state->modrm)));
        return FALSE;
      }
      if (state->inst->flags & NACL_IFLAG(OpcodeInModRmRm)) {
        if (modrm_rm(state->modrm) !=
            state->inst->opcode[state->inst->num_opcode_bytes+1]) {
          DEBUG(NaClLog(LOG_INFO,
                        "Discarding, opcode in mrm rm field (%02"NACL_PRIx8") "
                        "does not match\n",
                        modrm_rm(state->modrm)));
          return FALSE;
        }
      }
    }
  }
  return TRUE;
}

/* Returns true if the instruction requires a SIB bytes. */
static Bool NaClInstRequiresSib(NaClInstState* state) {
  /* Note: in 64-bit mode, 64-bit addressing is treated the same as 32-bit
   * addressing. Hence, required for all but 16-bit addressing, when
   * the right modrm bytes are specified.
   */
  return NaClInstRequiresModRm(state) && (16 != state->address_size) &&
      (modrm_rm(state->modrm) == 0x04 && modrm_mod(state->modrm) != 0x3);
}

/* Consume the SIB byte of the instruction, if applicable. Aborts the pattern
 * match if any problems are found.
 */
static Bool NaClConsumeSib(NaClInstState* state) {
  /* First check that the opcode (instruction) pattern specifies that
   * a SIB byte is needed, and that reading it will not walk past
   * the end of the code segment.
   */
  state->sib = 0;
  state->has_sib = NaClInstRequiresSib(state);
  DEBUG(NaClLog(LOG_INFO, "has sib = %d\n", (int) state->has_sib));
  if (state->has_sib) {
    if (state->length >= state->length_limit) {
      DEBUG(NaClLog(LOG_INFO, "Can't consume sib, no more bytes!\n"));
      return FALSE;
    }
    /* Read the SIB byte and record. */
    state->sib = state->mpc[state->length++];
    DEBUG(NaClLog(LOG_INFO, "sib = %02"NACL_PRIx8"\n", state->sib));
    if (sib_base(state->sib) == 0x05 && modrm_mod(state->modrm) > 2) {
      DEBUG(NaClLog(LOG_INFO,
                    "Sib byte implies modrm.mod field <= 2, match fails\n"));
      return FALSE;
    }
  }
  return TRUE;
}

static int NaClGetNumDispBytes(NaClInstState* state) {
  if (NaClInstRequiresModRm(state)) {
    if (16 == state->address_size) {
      /* Corresponding to table 2-1 of the Intel manual. */
      switch (modrm_mod(state->modrm)) {
        case 0x0:
          if (modrm_rm(state->modrm) == 0x06) {
            return 4;  /* disp16 */
          }
          break;
        case 0x1:
          return 1;    /* disp8 */
        case 0x2:
          return 2;    /* disp16 */
        default:
          break;
      }
    } else {
      /* Note: in 64-bit mode, 64-bit addressing is treated the same as 32-bit
       * addressing. Hence, this section covers the 32-bit addressing.
       */
      switch(modrm_mod(state->modrm)) {
        case 0x0:
          if (modrm_rm(state->modrm) == 0x05) {
            return 4;  /* disp32 */
          } else if (state->has_sib && sib_base(state->sib) == 0x5) {
            return 4;
          }
          break;
        case 0x1:
          return 1;    /* disp8 */
        case 0x2:
          return 4;    /* disp32 */
        default:
          break;
      }
    }
  }
  return 0;
}

/* Consume the needed displacement bytes, if applicable. Abort the
 * pattern match if any problems are found.
 */
static Bool NaClConsumeDispBytes(NaClInstState* state) {
  /* First check if the opcode (instruction) pattern specifies that
   * displacement bytes should be read, and that reading it will not
   * walk past the end of the code segment.
   */
  state->num_disp_bytes = NaClGetNumDispBytes(state);
  DEBUG(NaClLog(LOG_INFO,
                "num disp bytes = %"NACL_PRIu8"\n", state->num_disp_bytes));
  state->first_disp_byte = state->length;
  if (state->num_disp_bytes > 0) {
    int new_length = state->length + state->num_disp_bytes;
    if (new_length > state->length_limit) {
      DEBUG(NaClLog(LOG_INFO, "Can't consume disp, no more bytes!\n"));
      return FALSE;
    }
    /* Read the displacement bytes. */
    state->first_disp_byte = state->length;
    state->length = new_length;
  }
  return TRUE;
}

/* Returns the number of immediate bytes to parse. */
static int NaClGetNumImmedBytes(NaClInstState* state) {
  if (state->inst->flags & NACL_IFLAG(OpcodeHasImmed)) {
    return state->operand_size;
  }
  if (state->inst->flags & NACL_IFLAG(OpcodeHasImmed_v)) {
    return 4;
  } else if (state->inst->flags & NACL_IFLAG(OpcodeHasImmed_b)) {
    return 1;
  } else if (state->inst->flags & NACL_IFLAG(OpcodeHasImmed_w)) {
    return 2;
  } else if (state->inst->flags & NACL_IFLAG(OpcodeHasImmed_o)) {
    return 8;
  } else if (state->inst->flags & NACL_IFLAG(OpcodeHasImmed_Addr)) {
    return NaClExtractAddressSize(state) / 8;
  } else if (state->inst->flags & NACL_IFLAG(OpcodeHasImmed_z)) {
    if (state->operand_size == 2) {
      return 2;
    } else {
      return 4;
    }
  } else {
    return 0;
  }
}

/* Returns the number of immedidate bytes to parse if a second immediate
 * number appears in the instruction (zero if no second immediate value).
 */
static int NaClGetNumImmed2Bytes(NaClInstState* state) {
  if (state->inst->flags & NACL_IFLAG(OpcodeHasImmed2_b)) {
    return 1;
  } else if (state->inst->flags & NACL_IFLAG(OpcodeHasImmed2_w)) {
    return 2;
  } else if (state->inst->flags & NACL_IFLAG(OpcodeHasImmed2_v)) {
    return 4;
  } else {
    return 0;
  }
}

/* Consume the needed immediate bytes, if applicable. Abort the
 * pattern match if any problems are found.
 */
static Bool NaClConsumeImmediateBytes(NaClInstState* state) {
  /* find out how many immediate bytes are expected. */
  state->num_imm_bytes = NaClGetNumImmedBytes(state);
  DEBUG(NaClLog(LOG_INFO,
                "num immediate bytes = %"NACL_PRIu8"\n", state->num_imm_bytes));
  state->first_imm_byte = 0;
  if (state->num_imm_bytes > 0) {
    int new_length;
    /* Before reading immediate bytes, be sure that we won't walk
     * past the end of the code segment.
     */
    new_length = state->length + state->num_imm_bytes;
    if (new_length > state->length_limit) {
      DEBUG(NaClLog(LOG_INFO, "Can't consume immediate, no more bytes!\n"));
      return FALSE;
    }
    /* Read the immediate bytes. */
    state->first_imm_byte = state->length;
    state->length = new_length;
  }
  /* Before returning, see if second immediate value specified. */
  state->num_imm2_bytes = NaClGetNumImmed2Bytes(state);
  DEBUG(NaClLog(LOG_INFO, "num immediate 2 bytes = %"NACL_PRIu8"\n",
                state->num_imm2_bytes));
  if (state->num_imm2_bytes > 0) {
    int new_length;
    /* Before reading immediate bytes, be sure that we don't walk
     * past the end of the code segment.
     */
    new_length = state->length + state->num_imm2_bytes;
    if (new_length > state->length_limit) {
      DEBUG(NaClLog(LOG_INFO, "Can't consume 2nd immediate, no more bytes!\n"));
      return FALSE;
    }
    /* Read the immediate bytes. */
    state->length = new_length;
  }
  return TRUE;
}

/* Validate that any opcode (instruction) pattern prefix assumptions are
 * met by prefix bits. If not, abort the pattern match.
 */
static Bool NaClValidatePrefixFlags(NaClInstState* state) {
  /* Check lock prefix assumptions. */
  if (state->prefix_mask & kPrefixLOCK) {
    if (state->inst->flags & NACL_IFLAG(OpcodeLockable)) {
      /* Only allow if all destination operands are memory stores. */
      uint32_t i;
      Bool has_lockable_dest = FALSE;
      NaClExpVector* vector = NaClInstStateExpVector(state);
      DEBUG(NaClLog(LOG_INFO, "checking if lock valid on:\n");
            NaClExpVectorPrint(NaClLogGetGio(), vector));
      for (i = 0; i < vector->number_expr_nodes; ++i) {
        NaClExp* node = &vector->node[i];
        DEBUG(NaClLog(LOG_INFO, "  checking node %d\n", i));
        if ((NACL_EMPTY_EFLAGS != (node->flags & NACL_EFLAG(ExprDest))) &&
            (node->kind == ExprMemOffset)) {
          has_lockable_dest = TRUE;
          break;
        }
      }
      if (!has_lockable_dest) {
        DEBUG(NaClLog(LOG_INFO, "Instruction doesn't allow lock prefix "
                      "on non-memory destination"));
        return FALSE;
      }
    } else {
      DEBUG(NaClLog(LOG_INFO, "Instruction doesn't allow lock prefix\n"));
      return FALSE;
    }
  }
  /* Check REX prefix assumptions. */
  if (NACL_TARGET_SUBARCH == 64 &&
      (state->prefix_mask & kPrefixREX)) {
    if (state->inst->flags &
        (NACL_IFLAG(OpcodeUsesRexW) | NACL_IFLAG(OpcodeHasRexR) |
         NACL_IFLAG(OpcodeRex))) {
      if (((state->inst->flags & NACL_IFLAG(OpcodeUsesRexW)) &&
           0 == (state->rexprefix & 0x8)) ||
          ((state->inst->flags & NACL_IFLAG(OpcodeHasRexR)) &&
           0 == (state->rexprefix & 0x4))) {
        DEBUG(NaClLog(LOG_INFO, "can't match REX prefix requirement\n"));
        return FALSE;
      }
    }
  }
  return TRUE;
}

static void NaClClearInstState(NaClInstState* state, uint8_t opcode_length) {
  state->length = opcode_length;
  state->modrm = 0;
  state->has_sib = FALSE;
  state->sib = 0;
  state->num_disp_bytes = 0;
  state->first_disp_byte = 0;
  state->num_imm_bytes = 0;
  state->first_imm_byte = 0;
  state->num_imm2_bytes = 0;
  state->operand_size = 32;
  state->address_size = 32;
  state->nodes.is_defined = FALSE;
  state->nodes.number_expr_nodes = 0;
}

/*
 * Given the opcode prefix descriptor, return the list of candidate opcodes to
 * try and match against the byte stream in the given state. Before returning,
 * this function automatically advances the opcode prefix descriptor to describe
 * the next list to use if the returned list doesn't provide any matches.
 *
 * Parameters:
 *   state - The state of the instruction being decoded.
 *   desc - The description of how the opcode bytes have been matched.
 *      The value passed in is the currrent match, the value at exit is
 *      the value to be used the next time this function is called (to
 *      get the next set of possible instructions).
 *   opcode_length - The length (in bytes) of the opcode for the returned
 *       candidate opcodes.
 */
static NaClInst* NaClGetNextInstCandidates(NaClInstState* state,
                                            NaClInstPrefixDescriptor* desc,
                                            uint8_t* opcode_length) {
  NaClInst* cand_insts;
  if (desc->next_length_adjustment) {
    (*opcode_length) += desc->next_length_adjustment;
    desc->opcode_byte = state->mpc[*opcode_length - 1];
  }
  cand_insts = g_OpcodeTable[desc->matched_prefix][desc->opcode_byte];
  DEBUG(NaClLog(LOG_INFO, "Lookup candidates using [%s][%x]\n",
                NaClInstPrefixName(desc->matched_prefix), desc->opcode_byte));
  switch (desc->matched_prefix) {
    case Prefix660F:
      desc->matched_prefix = Prefix0F;
      break;
    case Prefix660F38:
      desc->matched_prefix = Prefix0F38;
      break;
    case Prefix660F3A:
      desc->matched_prefix = Prefix0F3A;
      break;
    case PrefixD8:
    case PrefixD9:
    case PrefixDA:
    case PrefixDB:
    case PrefixDC:
    case PrefixDD:
    case PrefixDE:
    case PrefixDF:
      desc->matched_prefix = NoPrefix;
      desc->next_length_adjustment = -1;
      break;
    default:
      /* No more simplier prefices, give up search after current lookup. */
      desc->matched_prefix = NaClInstPrefixEnumSize;
      break;
  }
  return cand_insts;
}

static Bool NaClConsumeOpcodeSequence(NaClInstState* state) {
  uint8_t next_byte;
  NaClInstNode* root;
  uint8_t orig_length;

  /* Cut quick if first byte not applicable. */
  if (state->length >= state->length_limit) return FALSE;
  next_byte = state->mpc[state->length];
  root = g_OpcodeSeq[0].succs[next_byte];
  if (NULL == root) return FALSE;
  DEBUG(NaClLog(LOG_INFO,
                "NaClConsume opcode char: %"NACL_PRIx8"\n", next_byte));

  /* If this point is reached, we are committed to attempting
   * a match, and must reset state if it fails.
   */
  orig_length = state->length;
  do {
    state->length++;
    if (NULL != root->matching_inst) {
      state->inst = root->matching_inst;
      DEBUG(NaClLog(LOG_INFO, "matched inst sequence!\n"));
      return TRUE;
    }
    next_byte = state->mpc[state->length];
    root = root->succs[next_byte];
    if (root == NULL) break;
    DEBUG(NaClLog(LOG_INFO,
                  "NaClConsume opcode char: %"NACL_PRIx8"\n", next_byte));
  } while (state->length <= state->length_limit);

  /* If reached, we updated the state, but did not find a match. Hence, revert
   * the state.
   */
  NaClClearInstState(state, orig_length);
  return FALSE;
}

/* Given the current location of the (relative) pc of the given instruction
 * iterator, update the given state to hold the (found) matched opcode
 * (instruction) pattern. If no matching pattern exists, set the state
 * to a matched undefined opcode (instruction) pattern. In all cases,
 * update the state to hold all information on the matched bytes of the
 * instruction. Note: The iterator expects that the opcode field is
 * changed from NULL to non-NULL by this fuction.
 */
void NaClDecodeInst(NaClInstIter* iter, NaClInstState* state) {
  uint8_t opcode_length = 0;
  NaClInst* cand_insts;
  Bool found_match = FALSE;
  /* Start by consuming the prefix bytes, and getting the possible
   * candidate opcode (instruction) patterns that can match, based
   * on the consumed opcode bytes.
   */
  NaClInstStateInit(iter, state);
  if (NaClConsumeOpcodeSequence(state)) {
    /* TODO(karl) Make this more general. Currently assumes that all
     * opcode sequences are nop's, and hence no additional processing
     * (other than opcode selection) is needed.
     */
    found_match = TRUE;
  } else if (NaClConsumePrefixBytes(state)) {
    NaClInstPrefixDescriptor prefix_desc;
    Bool continue_loop = TRUE;
    NaClConsumeInstBytes(state, &prefix_desc);
    opcode_length = state->length;
    while (continue_loop) {
      /* Try matching all possible candidates, in the order they are specified
       * (from the most specific prefix match, to the least specific prefix
       * match). Quit when the first pattern is matched.
       */
      if (prefix_desc.matched_prefix == NaClInstPrefixEnumSize) {
        continue_loop = FALSE;
      } else {
        cand_insts = NaClGetNextInstCandidates(state, &prefix_desc,
                                               &opcode_length);
        while (cand_insts != NULL) {
          NaClClearInstState(state, opcode_length);
          state->inst = cand_insts;
          DEBUG(NaClLog(LOG_INFO, "try opcode pattern:\n"));
          DEBUG(NaClInstPrint(NaClLogGetGio(), state->inst));
          if (NaClConsumeAndCheckOperandSize(state) &&
              NaClConsumeAndCheckAddressSize(state) &&
              NaClConsumeModRm(state) &&
              NaClConsumeSib(state) &&
              NaClConsumeDispBytes(state) &&
              NaClConsumeImmediateBytes(state) &&
              NaClValidatePrefixFlags(state)) {
            if (state->inst->flags & NACL_IFLAG(Opcode0F0F) &&
                /* Note: If all of the above code worked correctly,
                 * there should be no need for the following test.
                 * However, just to be safe, it is added.
                 */
                (state->num_imm_bytes == 1)) {
              /* Special 3DNOW instructions where opcode is in parsed
               * immediate byte at end of instruction. Look up in table,
               * and replace if found. Otherwise, let the default 0F0F lookup
               * act as the matching (invalid) instruction.
               */
              NaClInst* cand_inst;
              uint8_t opcode_byte = state->mpc[state->first_imm_byte];
              DEBUG(NaClLog(LOG_INFO,
                            "NaClConsume immediate byte opcode char: %"
                            NACL_PRIx8"\n", opcode_byte));
              cand_inst = g_OpcodeTable[Prefix0F0F][opcode_byte];
              if (NULL != cand_inst) {
                state->inst = cand_inst;
                DEBUG(NaClLog(LOG_INFO, "Replace with 3DNOW opcode:\n"));
                DEBUG(NaClInstPrint(NaClLogGetGio(), state->inst));
              }
            }
            /* found a match, exit loop. */
            found_match = TRUE;
            continue_loop = FALSE;
            break;
          } else {
            /* match failed, try next candidate pattern. */
            cand_insts = cand_insts->next_rule;
          }
        }
        DEBUG(if (! found_match) {
            NaClLog(LOG_INFO, "no more candidates for this prefix\n");
          });
      }
    }
  }

  /* If we did not match a defined opcode, match the undefined opcode,
   * forcing field opcode to be non-NULL.
   */
  if (!found_match) {
    DEBUG(NaClLog(LOG_INFO, "no instruction found, converting to undefined\n"));

    /* Can't figure out instruction, give up. */
    NaClClearInstState(state, opcode_length);
    state->inst = &g_Undefined_Opcode;
    if (state->length == 0 && state->length < state->length_limit) {
      /* Make sure we eat at least one byte. */
      ++state->length;
    }
  }
}

NaClInst* NaClInstStateInst(NaClInstState* state) {
  return state->inst;
}

NaClPcAddress NaClInstStateVpc(NaClInstState* state) {
  return state->vpc;
}

NaClExpVector* NaClInstStateExpVector(NaClInstState* state) {
  if (!state->nodes.is_defined) {
    state->nodes.is_defined = TRUE;
    NaClBuildExpVector(state);
  }
  return &state->nodes;
}

Bool NaClInstStateIsValid(NaClInstState* state) {
  return InstInvalid != state->inst->name;
}

uint8_t NaClInstStateLength(NaClInstState* state) {
  return state->length;
}

uint8_t NaClInstStateByte(NaClInstState* state, uint8_t index) {
  assert(index < state->length);
  return state->mpc[index];
}

uint8_t NaClInstStateOperandSize(NaClInstState* state) {
  return state->operand_size;
}

uint8_t NaClInstStateAddressSize(NaClInstState* state) {
  return state->address_size;
}

void NaClChangeOpcodesToXedsModel() {
  /* Changes opcodes to match xed. That is change:
   * 0f0f..1c: Pf2iw $Pq, $Qq => 0f0f..2c: Pf2iw $Pq, $Qq
   * 0f0f..1d: Pf2id $Pq, $Qq => 0f0f..2d: Pf2id $Pq, $Qq
   */
  g_OpcodeTable[Prefix0F0F][0x2c] = g_OpcodeTable[Prefix0F0F][0x1c];
  g_OpcodeTable[Prefix0F0F][0x1c] = NULL;
  g_OpcodeTable[Prefix0F0F][0x2d] = g_OpcodeTable[Prefix0F0F][0x1d];
  g_OpcodeTable[Prefix0F0F][0x1d] = NULL;
}

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
#include "native_client/src/trusted/validator_x86/nc_inst_iter.h"
#include "native_client/src/trusted/validator_x86/nc_inst_state_internal.h"
#include "native_client/src/trusted/validator_x86/nc_inst_trans.h"
#include "native_client/src/trusted/validator_x86/nc_segment.h"
#include "native_client/src/trusted/validator_x86/ncop_exps.h"
#include "native_client/src/trusted/validator_x86/ncopcode_desc.h"
#include "gen/native_client/src/trusted/validator_x86/nc_opcode_table.h"

/* To turn on debugging of instruction decoding, change value of
 * DEBUGGING to 1.
 */
#define DEBUGGING 0

#include "native_client/src/shared/utils/debugging.h"

/* Given the current location of the instruction iterator, initialize
 * the given state (to match).
 */
static void NcInstStateInit(NcInstIter* iter, NcInstState* state) {
  MemorySize limit;
  state->mpc = iter->segment->mbase + iter->index;
  state->vpc = iter->segment->vbase + iter->index;
  state->length = 0;
  limit = iter->segment->size - iter->index;
  if (limit > MAX_BYTES_PER_X86_INSTRUCTION) {
    limit = MAX_BYTES_PER_X86_INSTRUCTION;
  }
  state->length_limit = (uint8_t) limit;
  DEBUG(printf("length limit = %"PRIu8"\n", state->length_limit));
  state->num_prefix_bytes = 0;
  state->num_prefix_66 = 0;
  state->rexprefix = 0;
  state->prefix_mask = 0;
  state->opcode = NULL;
  state->is_nacl_legal = TRUE;
  state->nodes.number_expr_nodes = 0;
}

/* Computes the number of bytes defined for operands of the matched
 * instruction of the given state.
 */
static int ExtractOperandSize(NcInstState* state) {
  if (state->opcode->flags & InstFlag(OperandSize_b)) {
    return 1;
  }
  if (NACL_TARGET_SUBARCH == 64) {
    if ((state->rexprefix && state->rexprefix & 0x8) ||
        (state->opcode->flags & InstFlag(OperandSizeForce64))) {
      return 8;
    }
  }
  if (state->prefix_mask & kPrefixDATA16) {
    return (state->opcode->flags & InstFlag(OperandSizeIgnore66)) ? 4 : 2;
  }
  if (NACL_TARGET_SUBARCH == 64 &&
      (state->opcode->flags & InstFlag(OperandSizeDefaultIs64))) {
    return 8;
  }
  return 4;
}

/* Computes the number of bits defined for addresses of the matched
 * instruction of the given state.
 */
static int ExtractAddressSize(NcInstState* state) {
  if (NACL_TARGET_SUBARCH == 64) {
    if (state->opcode->flags & InstFlag(AddressSizeDefaultIs32)) {
      return (state->rexprefix & 0x8) ? 64 : 32;
    } else {
      return (state->prefix_mask & kPrefixADDR16) ? 32 : 64;
    }
  } else {
    return (state->prefix_mask & kPrefixADDR16) ? 16 : 32;
  }
}

/* Manual implies only 4 bytes is allowed, but I have found up to 6.
 * Why don't we allow any number, so long as (1) There is room for
 * at least one opcode byte, and (2) we don't exceed the max bytes.
 */
static const int kMaximumPrefixBytes = MAX_BYTES_PER_X86_INSTRUCTION - 1;

/* Match any prefix bytes that can be associated with the instruction
 * currently being matched.
 */
static Bool ConsumePrefixBytes(NcInstState* state) {
  uint8_t next_byte;
  int i;
  uint32_t prefix_form;
  int rex_index = -1;
  for (i = 0; i < kMaximumPrefixBytes; ++i) {
    /* Quit early if no more bytes in segment. */
    if (state->length >= state->length_limit) break;

    /* Look up the corresponding prefix bit associated
     * with the next byte in the segment, and record it.
     */
    next_byte = state->mpc[state->length];
    prefix_form = kPrefixTable[next_byte];
    if (prefix_form == 0) break;
    DEBUG(printf("Consume prefix[%d]: %02"PRIx8" => %"PRIx32"\n",
                 i, next_byte, prefix_form));
    state->prefix_mask |= prefix_form;
    if (kPrefixDATA16 == prefix_form) {
      ++state->num_prefix_66;
    }
    ++state->num_prefix_bytes;
    ++state->length;
    DEBUG(printf("  prefix mask: %08"PRIx32"\n", state->prefix_mask));

    /* If the prefix byte is a REX prefix, record its value, since
     * bits 5-8 of this prefix bit may be needed later.
     */
    if (NACL_TARGET_SUBARCH == 64) {
      if (prefix_form == kPrefixREX) {
        state->rexprefix = next_byte;
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
  OpcodePrefix matched_prefix;
  /* The number of bytes to subtract from the instruction length,
   * the next time GetNextOpcodeCandidates is called.
   */
  uint8_t next_length_adjustment;
} OpcodePrefixDescriptor;

/* Assuming we have matched the byte sequence OF 38, consume the corresponding
 * following (instruction) opcode byte, returning the most specific prefix the
 * patterns can match (or OpcodePrefixEnumSize if no such patterns exist);
 */
static void Consume0F38XXOpcodeBytes(NcInstState* state,
                                     OpcodePrefixDescriptor* desc) {
  /* Fail if there are no more bytes. Otherwise, read the next
   * byte.
   */
  if (state->length >= state->length_limit) {
    desc->matched_prefix = OpcodePrefixEnumSize;
    return;
  }

  desc->opcode_byte = state->mpc[state->length++];
  if (state->prefix_mask & kPrefixDATA16) {
    desc->matched_prefix = Prefix660F38;
  } else if (state->prefix_mask & kPrefixREPNE) {
    desc->matched_prefix = PrefixF20F38;
  } else if ((state->prefix_mask & ~kPrefixREX) == 0) {
    desc->matched_prefix = Prefix0F38;
  } else {
    /* Other prefixes like F3 cause an undefined instruction error. */
    desc->matched_prefix = OpcodePrefixEnumSize;
  }
}

/* Assuming we have matched the byte sequence OF 3A, consume the corresponding
 * following (instruction) opcode byte, returning the most specific prefix the
 * patterns can match (or OpcodePrefixEnumSize if no such patterns exist).
 */
static void Consume0F3AXXOpcodeBytes(NcInstState* state,
                                     OpcodePrefixDescriptor* desc) {
  /* Fail if there are no more bytes. Otherwise, read the next
   * byte and choose appropriate prefix.
   */
  if (state->length >= state->length_limit) {
    desc->matched_prefix = OpcodePrefixEnumSize;
    return;
  }

  desc->opcode_byte = state->mpc[state->length++];
  if (state->prefix_mask & kPrefixDATA16) {
    desc->matched_prefix = Prefix660F3A;
  } else if ((state->prefix_mask & ~kPrefixREX) == 0) {
    desc->matched_prefix = Prefix0F3A;
  } else {
    /* Other prefixes like F3 cause an undefined instruction error. */
    desc->matched_prefix = OpcodePrefixEnumSize;
  }
}

/* Assuming we have matched byte OF, consume the corresponding
 * following (instruction) opcode byte, returning the most specific
 * prefix the patterns can match (or OpcodePrefixEnumSize if no such
 * patterns exist).
 */
static void Consume0FXXOpcodeBytes(NcInstState* state,
                                   OpcodePrefixDescriptor* desc) {
  if (state->prefix_mask & kPrefixDATA16) {
    desc->matched_prefix = Prefix660F;
  } else if (state->prefix_mask & kPrefixREPNE) {
    desc->matched_prefix = PrefixF20F;
  } else if (state->prefix_mask & kPrefixREP) {
    desc->matched_prefix = PrefixF30F;
  } else {
    desc->matched_prefix = Prefix0F;
  }
}

/* Consume one of the x87 instructions that begin with D8-Df, and
 * match the most specific prefix pattern the opcode bytes can match.
 */
static void ConsumeX87OpcodeBytes(NcInstState* state,
                                  OpcodePrefixDescriptor* desc) {
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
 * the opcode bytes can match (or OpcodePrefixEnumSize if no such pattern
 * exists).
 */
static void ConsumeOpcodeBytes(NcInstState* state,
                               OpcodePrefixDescriptor* desc) {

  /* Initialize descriptor to the fail state. */
  desc->opcode_byte = 0x0;
  desc->matched_prefix = OpcodePrefixEnumSize;
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
          Consume0F38XXOpcodeBytes(state, desc);
          break;
        case 0x3a:
          Consume0F3AXXOpcodeBytes(state, desc);
          break;
        default:
          Consume0FXXOpcodeBytes(state, desc);
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
      ConsumeX87OpcodeBytes(state, desc);
      break;
    default:
      desc->matched_prefix = NoPrefix;
      break;
  }
  DEBUG(printf("matched prefix = %s\n",
               OpcodePrefixName(desc->matched_prefix)));
}

/* Compute the operand and address sizes for the instruction. Then, verify
 * that the opcode (instruction) pattern allows for such sizes. Aborts
 * the pattern match if any problems.
 */
static Bool ConsumeAndCheckOperandSize(NcInstState* state) {
  state->operand_size = ExtractOperandSize(state);
  DEBUG(printf("operand size = %"PRIu8"\n", state->operand_size));
  if (state->opcode->flags &
      (InstFlag(OperandSize_w) | InstFlag(OperandSize_v) |
       InstFlag(OperandSize_o))) {
    OpcodeFlags good = 1;
    switch (state->operand_size) {
      case 2:
        good = (state->opcode->flags & InstFlag(OperandSize_w));
        break;
      case 4:
        good = (state->opcode->flags & InstFlag(OperandSize_v));
        break;
      case 8:
        good = (state->opcode->flags & InstFlag(OperandSize_o));
        break;
      default:
        good = 0;
        break;
    }
    if (!good) {
      /* The flags associated with the opcode (instruction) don't
       * allow the computed sizes, abort the  match of the instruction.
       */
      DEBUG(printf("Operand size %"PRIu8" doesn't match flag requirement!\n",
                   state->operand_size));
      return FALSE;
    }
  }
  return TRUE;
}

static Bool ConsumeAndCheckAddressSize(NcInstState* state) {
  state->address_size = ExtractAddressSize(state);
  DEBUG(printf("Address size = %"PRIu8"\n", state->address_size));
  if (state->opcode->flags &
      (InstFlag(AddressSize_w) | InstFlag(AddressSize_v) |
       InstFlag(AddressSize_o))) {
    OpcodeFlags good = 1;
    switch (state->address_size) {
    case 16:
      good = (state->opcode->flags & InstFlag(AddressSize_w));
      break;
    case 32:
      good = (state->opcode->flags & InstFlag(AddressSize_v));
      break;
    case 64:
      good = (state->opcode->flags & InstFlag(AddressSize_o));
      break;
    default:
      good = 0;
      break;
    }
    if (!good) {
      /* The flags associated with the opcode (instruction) don't
       * allow the computed sizes, abort the  match of the instruction.
       */
      DEBUG(printf("Address size %"PRIu8" doesn't match flag requirement!\n",
                   state->address_size));
      return FALSE;
    }
  }
  return TRUE;
}

/* Returns true if the instruction requires a ModRm bytes. */
static Bool InstructionRequiresModRm(NcInstState* state) {
  return
      ((OpcodeFlag) 0) !=
      (state->opcode->flags &
       (InstFlag(OpcodeUsesModRm) | InstFlag(OpcodeInModRm)));
}

/* Consume the Mod/Rm byte of the instruction, if applicable.
 * Aborts the pattern match if any problems.
 */
static Bool ConsumeModRm(NcInstState* state) {
  /* First check if the opcode (instruction) pattern specifies that
   * a Mod/Rm byte is needed, and that reading it will not walk
   * past the end of the code segment.
   */
  if (InstructionRequiresModRm(state)) {
    uint8_t byte;
    /* Has modrm byte. */
    if (state->length >= state->length_limit) {
      DEBUG(printf("Can't read mod/rm, no more bytes!\n"));
      return FALSE;
    }
    byte = state->mpc[state->length];
    /* Note: 0xC0 is a limit added by floating point instructions,
     * and is marked using the ModRmLessThanC0 flag. Check for
     * this case, and quit if the condition is violated.
     */
    if ((state->opcode->flags & InstFlag(OpcodeLtC0InModRm)) &&
        byte >= 0xC0) {
      DEBUG(printf("Can't read x87 mod/rm value, %"PRIx8" not < 0xC0\n",
                   byte));
      return FALSE;
    }
    /* Note: Some instructions only allow values where the ModRm mod field
     * is 0x3.
     */
    if ((state->opcode->flags & InstFlag(ModRmModIs0x3)) &&
        modrm_mod(byte) != 0x3) {
      DEBUG(printf("Can't match, modrm mod field not 0x3\n"));
      return FALSE;
    }
    state->modrm = byte;
    state->length++;
    state->num_disp_bytes = 0;
    state->first_disp_byte = 0;
    state->sib = 0;
    state->has_sib = FALSE;
    DEBUG(printf("consume modrm = %02"PRIx8"\n", state->modrm));

    /* Consume the remaining opcode value in the mod/rm byte
     * if applicable.
     */
    if (state->opcode->flags & InstFlag(OpcodeInModRm)) {
      /* TODO(karl) Optimize this with faster match on
       * opcode value in first operand.
       */
      if (modrm_opcode(state->modrm) !=
          (state->opcode->operands[0].kind - Opcode0)) {
        DEBUG(
            printf(
                "Discarding, opcode in mrm byte (%02"PRIx8") does not match\n",
                modrm_opcode(state->modrm)));
        return FALSE;
      }
    }
  }
  return TRUE;
}

/* Returns true if the instruction requires a SIB bytes. */
static Bool InstructionRequiresSib(NcInstState* state) {
  /* Note: in 64-bit mode, 64-bit addressing is treated the same as 32-bit
   * addressing. Hence, required for all but 16-bit addressing, when
   * the right modrm bytes are specified.
   */
  return InstructionRequiresModRm(state) && (16 != state->address_size) &&
      (modrm_rm(state->modrm) == 0x04 && modrm_mod(state->modrm) != 0x3);
}

/* Consume the SIB byte of the instruction, if applicable. Aborts the pattern
 * match if any problems are found.
 */
static Bool ConsumeSib(NcInstState* state) {
  /* First check that the opcode (instruction) pattern specifies that
   * a SIB byte is needed, and that reading it will not walk past
   * the end of the code segment.
   */
  state->sib = 0;
  state->has_sib = InstructionRequiresSib(state);
  DEBUG(printf("has sib = %d\n", (int) state->has_sib));
  if (state->has_sib) {
    if (state->length >= state->length_limit) {
      DEBUG(printf("Can't consume sib, no more bytes!\n"));
      return FALSE;
    }
    /* Read the SIB byte and record. */
    state->sib = state->mpc[state->length++];
    DEBUG(printf("sib = %02"PRIx8"\n", state->sib));
    if (sib_base(state->sib) == 0x05 && modrm_mod(state->modrm) > 2) {
      DEBUG(printf("Sib byte implies modrm.mod field <= 2, match fails\n"));
      return FALSE;
    }
  }
  return TRUE;
}

static int GetNumDispBytes(NcInstState* state) {
  if (InstructionRequiresModRm(state)) {
    if (16 == state->address_size) {
      /* Corresponding to table 2-1 of the Intel manual. */
      switch (modrm_mod(state->modrm)) {
        case 0x0:
          if (modrm_rm(state->modrm) == 0x06) {
            return 32;  /* disp16 */
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
static Bool ConsumeDispBytes(NcInstState* state) {
  /* First check if the opcode (instruction) pattern specifies that
   * displacement bytes should be read, and that reading it will not
   * walk past the end of the code segment.
   */
  state->num_disp_bytes = GetNumDispBytes(state);
  DEBUG(printf("num disp bytes = %"PRIu8"\n", state->num_disp_bytes));
  state->first_disp_byte = state->length;
  if (state->num_disp_bytes > 0) {
    int new_length = state->length + state->num_disp_bytes;
    if (new_length > state->length_limit) {
      DEBUG(printf("Can't consume disp, no more bytes!\n"));
      return FALSE;
    }
    /* Read the displacement bytes. */
    state->first_disp_byte = state->length;
    state->length = new_length;
  }
  return TRUE;
}

/* Returns the number of immediate bytes to parse. */
static int GetNumImmediateBytes(NcInstState* state) {
  if (state->opcode->flags & InstFlag(OpcodeHasImmed)) {
    return state->operand_size;
  }
  if (state->opcode->flags & InstFlag(OpcodeHasImmed_v)) {
    return 4;
  } else if (state->opcode->flags & InstFlag(OpcodeHasImmed_b)) {
    return 1;
  } else if (state->opcode->flags & InstFlag(OpcodeHasImmed_w)) {
    return 2;
  } else if (state->opcode->flags & InstFlag(OpcodeHasImmed_o)) {
    return 8;
  } else if (state->opcode->flags & InstFlag(OpcodeHasImmed_Addr)) {
    return ExtractAddressSize(state) / 8;
  } else {
    return 0;
  }
}

/* Returns the number of immedidate bytes to parse if a second immediate
 * number appears in the instruction (zero if no second immediate value).
 */
static int GetNumImmediate2Bytes(NcInstState* state) {
  if (state->opcode->flags & InstFlag(OpcodeHasImmed2_b)) {
    return 1;
  } else if (state->opcode->flags & InstFlag(OpcodeHasImmed2_w)) {
    return 2;
  } else if (state->opcode->flags & InstFlag(OpcodeHasImmed2_v)) {
    return 4;
  } else {
    return 0;
  }
}

/* Consume the needed immediate bytes, if applicable. Abort the
 * pattern match if any problems are found.
 */
static Bool ConsumeImmediateBytes(NcInstState* state) {
  /* find out how many immediate bytes are expected. */
  state->num_imm_bytes = GetNumImmediateBytes(state);
  DEBUG(printf("num immediate bytes = %"PRIu8"\n", state->num_imm_bytes));
  state->first_imm_byte = 0;
  if (state->num_imm_bytes > 0) {
    int new_length;
    /* Before reading immediate bytes, be sure that we won't walk
     * past the end of the code segment.
     */
    new_length = state->length + state->num_imm_bytes;
    if (new_length > state->length_limit) {
      DEBUG(printf("Can't consume immediate, no more bytes!\n"));
      return FALSE;
    }
    /* Read the immediate bytes. */
    state->first_imm_byte = state->length;
    state->length = new_length;
  }
  /* Before returning, see if second immediate value specified. */
  state->num_imm2_bytes = GetNumImmediate2Bytes(state);
  DEBUG(printf("num immediate 2 bytes = %"PRIu8"\n", state->num_imm2_bytes));
  if (state->num_imm2_bytes > 0) {
    int new_length;
    /* Before reading immediate bytes, be sure that we don't walk
     * past the end of the code segment.
     */
    new_length = state->length + state->num_imm2_bytes;
    if (new_length > state->length_limit) {
      DEBUG(printf("Can't consume 2nd immediate, no more bytes!\n"));
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
static Bool ValidatePrefixFlags(NcInstState* state) {
  /* Check lock prefix assumptions. */
  if ((state->prefix_mask & kPrefixLOCK) &&
      0 == (state->opcode->flags & InstFlag(OpcodeLockable))) {
    DEBUG(printf("Instruction doesn't allow lock prefix\n"));
    return FALSE;
  }
  /* Check REX prefix assumptions. */
  if (NACL_TARGET_SUBARCH == 64 &&
      (state->prefix_mask & kPrefixREX)) {
    if (state->opcode->flags &
        (InstFlag(OpcodeUsesRexW) | InstFlag(OpcodeHasRexR) |
         InstFlag(OpcodeRex))) {
      if (((state->opcode->flags & InstFlag(OpcodeUsesRexW)) &&
           0 == (state->rexprefix & 0x8)) ||
          ((state->opcode->flags & InstFlag(OpcodeHasRexR)) &&
           0 == (state->rexprefix & 0x4))) {
        DEBUG(printf("can't match REX prefix requirement\n"));
        return FALSE;
      }
    }
  }
  return TRUE;
}

static void ClearOpcodeState(NcInstState* state, uint8_t opcode_length,
                             Bool is_nacl_legal) {
  state->length = opcode_length;
  state->is_nacl_legal = is_nacl_legal;
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
static Opcode* GetNextOpcodeCandidates(NcInstState* state,
                                    OpcodePrefixDescriptor* desc,
                                    uint8_t* opcode_length) {
  Opcode* cand_opcodes;
  if (desc->next_length_adjustment) {
    (*opcode_length) += desc->next_length_adjustment;
    desc->opcode_byte = state->mpc[*opcode_length - 1];
  }
  cand_opcodes = g_OpcodeTable[desc->matched_prefix][desc->opcode_byte];
  DEBUG(printf("Lookup candidates using [%s][%x]\n",
               OpcodePrefixName(desc->matched_prefix), desc->opcode_byte));
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
      desc->matched_prefix = OpcodePrefixEnumSize;
      break;
  }
  return cand_opcodes;
}

static Bool ConsumeOpcodeSequence(NcInstState* state) {
  uint8_t next_byte;
  NcOpcodeNode* root;
  uint8_t orig_length;
  Bool orig_legal;

  /* Cut quick if first byte not applicable. */
  if (state->length >= state->length_limit) return FALSE;
  next_byte = state->mpc[state->length];
  root = g_OpcodeSeq[0].succs[next_byte];
  if (NULL == root) return FALSE;
  DEBUG(printf("Consume opcode char: %"PRIx8"\n", next_byte));

  /* If this point is reached, we are committed to attempting
   * a match, and must reset state if it fails.
   */
  orig_length = state->length;
  orig_legal = state->is_nacl_legal;
  do {
    state->length++;
    if (NULL != root->matching_opcode) {
      state->opcode = root->matching_opcode;
      DEBUG(printf("matched opcode sequence!\n"));
      return TRUE;
    }
    next_byte = state->mpc[state->length];
    root = root->succs[next_byte];
    if (root == NULL) break;
    DEBUG(printf("Consume opcode char: %"PRIx8"\n", next_byte));
  } while (state->length <= state->length_limit);

  /* If reached, we updated the state, but did not find a match. Hence, revert
   * the state.
   */
  ClearOpcodeState(state, orig_length, orig_legal);
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
void DecodeInstruction(
    NcInstIter* iter,
    NcInstState* state) {
  uint8_t opcode_length = 0;
  Opcode* cand_opcodes;
  Bool is_nacl_legal = FALSE;
  Bool found_match = FALSE;
  /* Start by consuming the prefix bytes, and getting the possible
   * candidate opcode (instruction) patterns that can match, based
   * on the consumed opcode bytes.
   */
  NcInstStateInit(iter, state);
  if (ConsumeOpcodeSequence(state)) {
    /* TODO(karl) Make this more general. Currently assumes that all
     * opcode sequences are nop's, and hence no additional processing
     * (other than opcode selection) is needed.
     */
    is_nacl_legal = TRUE;
    found_match = TRUE;
  } else if (ConsumePrefixBytes(state)) {
    OpcodePrefixDescriptor prefix_desc;
    Bool continue_loop = TRUE;
    ConsumeOpcodeBytes(state, &prefix_desc);
    opcode_length = state->length;
    is_nacl_legal = state->is_nacl_legal;
    while (continue_loop) {
      /* Try matching all possible candidates, in the order they are specified
       * (from the most specific prefix match, to the least specific prefix
       * match). Quit when the first pattern is matched.
       */
      if (prefix_desc.matched_prefix == OpcodePrefixEnumSize) {
        continue_loop = FALSE;
      } else {
        cand_opcodes = GetNextOpcodeCandidates(state, &prefix_desc,
                                               &opcode_length);
        while (cand_opcodes != NULL) {
          ClearOpcodeState(state, opcode_length, is_nacl_legal);
          state->opcode = cand_opcodes;
          DEBUG(printf("try opcode pattern:\n"));
          DEBUG(PrintOpcode(stdout, state->opcode));
          if (ConsumeAndCheckOperandSize(state) &&
              ConsumeAndCheckAddressSize(state) &&
              ConsumeModRm(state) &&
              ConsumeSib(state) &&
              ConsumeDispBytes(state) &&
              ConsumeImmediateBytes(state) &&
              ValidatePrefixFlags(state)) {
            /* found a match, exit loop. */
            found_match = TRUE;
            continue_loop = FALSE;
            break;
          } else {
            /* match failed, try next candidate pattern. */
            cand_opcodes = cand_opcodes->next_rule;
          }
        }
        DEBUG(if (! found_match) {
            printf("no more candidates for this prefix\n");
          });
      }
    }
  }

  /* If we did not match a defined opcode, match the undefined opcode,
   * forcing field opcode to be non-NULL.
   */
  if (!found_match) {
    DEBUG(printf("no instruction found, converting to undefined\n"));

    /* Can't figure out instruction, give up. */
    ClearOpcodeState(state, opcode_length, is_nacl_legal);
    state->opcode = &g_Undefined_Opcode;
    if (state->length == 0 && state->length < state->length_limit) {
      /* Make sure we eat at least one byte. */
      ++state->length;
    }
  }
  /* Update nacl legal flag based on opcode information. */
  if (state->is_nacl_legal) {
    /* Don't allow more than one (non-REX) prefix. */
    int num_prefix_bytes = state->num_prefix_bytes;
    if (state->rexprefix) --num_prefix_bytes;

    /* Don't count prefix bytes that explicitly state to ignore,
     * such as the nop instruction.
     */
    if (state->opcode->flags & InstFlag(IgnorePrefixDATA16)) {
      if (state->num_prefix_66 > 0) {
        num_prefix_bytes -= state->num_prefix_66;
      }
    }

    if (num_prefix_bytes > 1) {
      state->is_nacl_legal = FALSE;
    }
    switch (state->opcode->insttype) {
      case NACLi_UNDEFINED:
      case NACLi_ILLEGAL:
      case NACLi_INVALID:
      case NACLi_SYSTEM:
        state->is_nacl_legal = FALSE;
        break;
      default:
        break;
    }
    if (InstFlag(NaclIllegal) & state->opcode->flags) {
      state->is_nacl_legal = FALSE;
    }
    if (NACL_TARGET_SUBARCH == 64) {
      /* Don't allow CS, DS, ES, or SS prefix overrides,
       * since it has no effect, unless explicitly stated
       * otherwise.
       */
      if (state->prefix_mask &
          (kPrefixSEGCS | kPrefixSEGSS | kPrefixSEGES |
           kPrefixSEGDS)) {
        if ((OpcodeFlags)0 ==
            (state->opcode->flags & InstFlag(IgnorePrefixSEGCS))) {
          state->is_nacl_legal = FALSE;
        }
      }
    }
  }
}

Opcode* NcInstStateOpcode(NcInstState* state) {
  return state->opcode;
}

PcAddress NcInstStateVpc(NcInstState* state) {
  return state->vpc;
}

ExprNodeVector* NcInstStateNodeVector(NcInstState* state) {
  if (state->nodes.number_expr_nodes == 0) {
    BuildExprNodeVector(state);
  }
  return &state->nodes;
}

Bool NcInstStateIsNaclLegal(NcInstState* state) {
  return state->is_nacl_legal;
}

uint8_t NcInstStateLength(NcInstState* state) {
  return state->length;
}

uint8_t NcInstStateByte(NcInstState* state, uint8_t index) {
  assert(index < state->length);
  return state->mpc[index];
}

uint8_t NcInstStateOperandSize(NcInstState* state) {
  return state->operand_size;
}

uint8_t NcInstStateAddressSize(NcInstState* state) {
  return state->address_size;
}

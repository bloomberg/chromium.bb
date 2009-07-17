/*
 * Copyright 2009, Google Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
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

#if DEBUGGING
/* Defines to execute statement(s) s if in debug mode. */
#define DEBUG(s) s
#else
/* Defines to not include statement(s) s if not in debugging mode. */
#define DEBUG(s) do { if (0) { s; } } while(0)
#endif

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
  state->rexprefix = 0;
  state->prefix_mask = 0;
  state->opcode = NULL;
  state->is_nacl_legal = TRUE;
  state->nodes.number_expr_nodes = 0;
  state->workspace.next_available = 0;
}

/* Computes the number of bytes defined for operands of the matched
 * instruction of the given state.
 */
static int ExtractOperandSize(NcInstState* state) {
  if (NACL_TARGET_SUBARCH == 64) {
    if ((state->rexprefix && state->rexprefix & 0x8) ||
        (state->opcode->flags & InstFlag(OperandSizeForce64))) {
      return 8;
    }
  }
  if (state->prefix_mask & kPrefixDATA16) {
    return 2;
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
    return (state->prefix_mask & kPrefixADDR16) ? 32 : 64;
  } else {
    return (state->prefix_mask & kPrefixADDR16) ? 16 : 32;
  }
}

/* Match any prefix bytes that can be associated with the instruction
 * currently being matched.
 */
static Bool ConsumePrefixBytes(NcInstState* state) {
  uint8_t next_byte;
  int i;
  uint32_t prefix_form;
  int lock_index = -1;
  int rex_index = -1;
  for (i = 0; i < kMaxPrefixBytes; ++i) {
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
      } else if (prefix_form == kPrefixLOCK) {
        lock_index = i;
      }
    }
  }
  if (NACL_TARGET_SUBARCH == 64) {
    /* REX prefix must be last, unless FO exists. If FO
     * exists, it must be after REX.
     */
    if (rex_index >= 0) {
      if (lock_index >= 0) {
        return ((rex_index + 1) == lock_index) &&
          ((lock_index + 1) == state->num_prefix_bytes);
      } else {
        return (rex_index + 1) == state->num_prefix_bytes;
      }
    }
  }
  return TRUE;
}

/* Assuming we have matched the byte sequence OF 38, consume the corresponding
 * following (instruction) opcode byte, returning the possible list of
 * patterns that may match (or NULL if no such patterns).
 */
static Opcode* Consume0F38XXOpcodeBytes(NcInstState* state) {
  /* Fail if there are no more bytes. Otherwise, read the next
   * byte.
   */
  uint8_t opcode_byte;
  if (state->length >= state->length_limit) return NULL;
  opcode_byte = state->mpc[state->length++];

  /* TODO(karl) figure out if we need to encode prefix bytes,
   * or if the opcode flags do the same thing.
   */
  if (state->prefix_mask & kPrefixDATA16) {
    return g_OpcodeTable[Prefix660F38][opcode_byte];
  } else if (state->prefix_mask & kPrefixREPNE) {
    return g_OpcodeTable[PrefixF20F38][opcode_byte];
  } else if ((state->prefix_mask & ~kPrefixREX) == 0) {
    return g_OpcodeTable[Prefix0F38][opcode_byte];
  } else {
    /* Other prefixes like F3 cause an undefined instruction error. */
    return NULL;
  }
  /* NOT REACHED */
  return NULL;
}

/* Assuming we have matched the byte sequence OF 3A, consume the corresponding
 * following (instruction) opcode byte, returning the possible list of
 * patterns that may match (or NULL if no such patterns).
 */
static Opcode* Consume0F3AXXOpcodeBytes(NcInstState* state) {
  /* Fail if there are no more bytes. Otherwise, read the next
   * byte.
   */
  uint8_t opcode_byte;
  if (state->length >= state->length_limit) return NULL;
  opcode_byte = state->mpc[state->length++];

  /* TODO(karl) figure out if we need to encode prefix bytes,
   * or if the opcode flags do the same thing.
   */
  if (state->prefix_mask & kPrefixDATA16) {
    return g_OpcodeTable[Prefix660F3A][opcode_byte];
  } else if ((state->prefix_mask & ~kPrefixREX) == 0) {
    return g_OpcodeTable[Prefix0F3A][opcode_byte];
  } else {
    /* Other prefixes like F3 cause an undefined instruction error. */
    return NULL;
  }
  /* NOT REACHED */
  return NULL;
}

/* Assuming we have matched byte OF, consume the corresponding
 * following (instruction) opcode byte, returning the possible list of
 * patterns that may match (or NULL if no such pattern).
 */
static Opcode* Consume0FXXOpcodeBytes(NcInstState* state, uint8_t opcode_byte) {
  /* TODO(karl) figure out if we need to encode prefix bytes,
   * or if the opcode flags do the same thing.
   */
  if (state->prefix_mask & kPrefixDATA16) {
    return g_OpcodeTable[Prefix66OF][opcode_byte];
  } else if (state->prefix_mask & kPrefixREPNE) {
    return g_OpcodeTable[PrefixF20F][opcode_byte];
  } else if (state->prefix_mask & kPrefixREP) {
    return g_OpcodeTable[PrefixF30F][opcode_byte];
  } else {
    return g_OpcodeTable[Prefix0F][opcode_byte];
  }
  /* NOT REACHED */
  return NULL;
}

/* Consume the sequence of bytes corresponding to the 1-3 byte opcode.
 * Return the list of opcode (instruction) patterns that apply to
 * the matched instruction bytes (or NULL if no such patterns).
 */
static Opcode* ConsumeOpcodeBytes(NcInstState* state) {
  uint8_t opcode_byte;
  Opcode* cand_opcodes;

  /* Be sure that we don't exceed the segment length. */
  if (state->length >= state->length_limit) return NULL;

  /* Record the opcode(s) we matched. */
  opcode_byte = state->mpc[state->length++];
  if (opcode_byte == 0x0F) {
    uint8_t opcode_byte2;
    if (state->length >= state->length_limit) return NULL;
    opcode_byte2 = state->mpc[state->length++];
    switch (opcode_byte2) {
    case 0x38:
      cand_opcodes = Consume0F38XXOpcodeBytes(state);
      break;
    case 0x3a:
      cand_opcodes = Consume0F3AXXOpcodeBytes(state);
      break;
    default:
      cand_opcodes = Consume0FXXOpcodeBytes(state, opcode_byte2);
      break;
    }
  } else {
    cand_opcodes = g_OpcodeTable[NoPrefix][opcode_byte];
  }
  state->opcode = cand_opcodes;
  if (NULL != cand_opcodes) {
    DEBUG(printf("opcode pattern:\n"));
    DEBUG(PrintOpcode(stdout, state->opcode));
  }
  return cand_opcodes;
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
  return state->opcode->flags &
      (InstFlag(OpcodeUsesModRm) | InstFlag(OpcodeInModRm));
}

/* Consume the Mod/Rm byte of the instruction, if applicable.
 * Aborts the pattern match if any problems.
 */
static Bool ConsumeModRm(NcInstState* state) {
  /* First check if the opcode (instruction) pattern specifies that
   * a Mod/Rm byte is needed, and that reading it will not walk
   * past the end of the code segment.
   */
  if (state->opcode->flags &
      (InstFlag(OpcodeUsesModRm) | InstFlag(OpcodeInModRm))) {
    /* Has modrm byte. */
    if (state->length >= state->length_limit) {
      DEBUG(printf("Can't read mod/rm, no more bytes!\n"));
      return FALSE;
    }
    state->modrm = state->mpc[state->length++];
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
    DEBUG(printf("sib = %02"PRId8"\n", state->sib));
    if (sib_base(state->sib) == 0x05 && modrm_mod(state->modrm) > 2) {
      DEBUG(printf("Sib byte implies modrm.mod field <= 2, match fails\n"));
      return FALSE;
    }
  }
  return TRUE;
}

static int GetNumDispBytes(NcInstState* state) {
  if (16 == state->address_size) {
    /* Corresponding to table 2-1 of the Intel manual. */
    switch (modrm_mod(state->modrm)) {
      case 0x0:
        if (modrm_rm(state->modrm) == 0x06) {
          return 2;  /* disp16 */
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
  if (state->opcode->flags & InstFlag(OpcodeIv)) {
    return 4;
  } else if (state->opcode->flags & InstFlag(OpcodeIb)) {
    return 1;
  } else if (state->opcode->flags & InstFlag(OpcodeIw)) {
    return 2;
  } else if (state->opcode->flags & InstFlag(OpcodeIo)) {
    return 8;
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
  state->operand_size = 32;
  state->address_size = 32;
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
  uint8_t opcode_length;
  Opcode* cand_opcodes;
  Bool is_nacl_legal;
  /* Start by consuming the prefix bytes, and getting the possible
   * candidate opcode (instruction) patterns that can match, based
   * on the consumed opcode bytes.
   */
  NcInstStateInit(iter, state);
  if (ConsumePrefixBytes(state)) {
    cand_opcodes = ConsumeOpcodeBytes(state);
    /* Try matching all possible candidates, in the order they are specified.
     * Quit when the first pattern is matched.
     */
    opcode_length = state->length;
    is_nacl_legal = state->is_nacl_legal;
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
        break;
      } else {
        /* match failed, try next candidate pattern. */
        cand_opcodes = cand_opcodes->next_rule;
      }
    }
  }

  /* If we did not match a defined opcode, match the undefined opcode,
   * forcing field opcode to be non-NULL.
   */
  if (NULL == state->opcode) {
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

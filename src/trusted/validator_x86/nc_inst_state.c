/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * Defines an instruction (decoder), based on the current location of
 * the instruction iterator. The instruction decoder takes a list
 * of candidate opcode (instruction) patterns, and searches for the
 * first candidate that matches the requirements of the opcode pattern.
 */


#include "native_client/src/trusted/validator_x86/nc_inst_state.h"

/* To turn on debugging of instruction decoding, change value of
 * DEBUGGING to 1.
 */
#define DEBUGGING 0

#include "native_client/src/trusted/validator_x86/nc_inst_state_statics.c"

/* Given the current location of the (relative) pc of the given instruction
 * iterator, update the given state to hold the (found) matched opcode
 * (instruction) pattern. If no matching pattern exists, set the state
 * to a matched undefined opcode (instruction) pattern. In all cases,
 * update the state to hold all information on the matched bytes of the
 * instruction. Note: The iterator expects that the opcode field is
 * changed from NULL to non-NULL by this fuction.
 */
void NaClDecodeInst(NaClInstIter* iter, NaClInstState* state) {
  uint8_t inst_length = 0;
  const NaClInst* cand_insts;
  Bool found_match = FALSE;
  /* Start by consuming the prefix bytes, and getting the possible
   * candidate opcode (instruction) patterns that can match, based
   * on the consumed opcode bytes.
   */
  NaClInstStateInit(iter, state);
  if (NaClConsumeOpcodeSequence(state)) {
    found_match = TRUE;
  } else if (NaClConsumePrefixBytes(state)) {
    NaClInstPrefixDescriptor prefix_desc;
    Bool continue_loop = TRUE;
    NaClConsumeInstBytes(state, &prefix_desc);
    inst_length = state->bytes.length;
    while (continue_loop) {
      /* Try matching all possible candidates, in the order they are specified
       * (from the most specific prefix match, to the least specific prefix
       * match). Quit when the first pattern is matched.
       */
      if (prefix_desc.matched_prefix == NaClInstPrefixEnumSize) {
        continue_loop = FALSE;
      } else {
        uint8_t cur_opcode_prefix = prefix_desc.opcode_prefix;
        cand_insts = NaClGetNextInstCandidates(state, &prefix_desc,
                                               &inst_length);
        while (cand_insts != NULL) {
          NaClClearInstState(state, inst_length);
          state->inst = cand_insts;
          DEBUG(NaClLog(LOG_INFO, "try opcode pattern:\n"));
          DEBUG_OR_ERASE(NaClInstPrint(NaClLogGetGio(), state->decoder_tables,
                                       state->inst));
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
              const NaClInst* cand_inst;
              uint8_t opcode_byte = state->bytes.byte[state->first_imm_byte];
              DEBUG(NaClLog(LOG_INFO,
                            "NaClConsume immediate byte opcode char: %"
                            NACL_PRIx8"\n", opcode_byte));
              cand_inst = NaClGetPrefixOpcodeInst(state->decoder_tables,
                                                  Prefix0F0F, opcode_byte);
              if (NULL != cand_inst) {
                state->inst = cand_inst;
                DEBUG(NaClLog(LOG_INFO, "Replace with 3DNOW opcode:\n"));
                DEBUG_OR_ERASE(NaClInstPrint(NaClLogGetGio(),
                                             state->decoder_tables,
                                             state->inst));
              }
            }
            /* found a match, exit loop. */
            found_match = TRUE;
            continue_loop = FALSE;
            state->opcode_prefix = cur_opcode_prefix;
            state->num_opcode_bytes = inst_length - state->num_prefix_bytes;
            break;
          } else {
            /* match failed, try next candidate pattern. */
            cand_insts = NaClGetOpcodeInst(state->decoder_tables,
                                           cand_insts->next_rule);
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
    NaClClearInstState(state, inst_length);
    state->inst = state->decoder_tables->undefined;
    if (state->bytes.length == 0 && state->bytes.length < state->length_limit) {
      /* Make sure we eat at least one byte. */
      NCInstBytesRead(&state->bytes);
    }
  }
}

const NaClInst* NaClInstStateInst(NaClInstState* state) {
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
  return state->bytes.length;
}

uint8_t NaClInstStateByte(NaClInstState* state, uint8_t index) {
  assert(index < state->bytes.length);
  return state->bytes.byte[index];
}

uint8_t NaClInstStateOperandSize(NaClInstState* state) {
  return state->operand_size;
}

uint8_t NaClInstStateAddressSize(NaClInstState* state) {
  return state->address_size;
}

/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef NACL_TRUSTED_BUT_NOT_TCB
#error("This file is not meant for use in the TCB")
#endif

#include "native_client/src/trusted/validator_x86/ncdis_segments.h"

#include <string.h>

#include "native_client/src/shared/platform/nacl_log.h"
#include "native_client/src/trusted/validator_x86/ncdis_util.h"
#include "native_client/src/trusted/validator_x86/ncop_exps.h"
#include "native_client/src/trusted/validator_x86/nc_inst_iter.h"
#include "native_client/src/trusted/validator_x86/nc_inst_state_internal.h"
#include "native_client/src/trusted/validator_x86/nc_segment.h"

Bool NACL_FLAGS_use_iter = (64 == NACL_TARGET_SUBARCH);

Bool NACL_FLAGS_internal = FALSE;

static const char* kHardCodedMessage = "[hard coded]";

/* Inspect the parsed instruction to print out the opcode sequence matched. */
static void NaClInstPrintOpcodeSeq(struct Gio* gout,
                                   const NaClInstState* state) {
  size_t count = 0;
  if (state->num_opcode_bytes == 0) {
    /* Hard coded bytes sequence for instruction. */
    gprintf(gout, "  %s", kHardCodedMessage);
    count = strlen(kHardCodedMessage) + 2;
  } else {
    /* Modeled instruction. Pull out parsed opcode bytes from parsed
     * instruction.
     */
    int i;
    gprintf(gout, " ");
    count = 1;

    /* Add prefix selector if applicable. */
    if (state->opcode_prefix) {
      gprintf(gout, " %02x", state->opcode_prefix);
      count += 3;
    }

    /* Add opcode bytes. */
    for (i = 0; i < state->num_opcode_bytes; ++i) {
      gprintf(gout, " %02x", state->bytes.byte[state->num_prefix_bytes + i]);
      count += 3;
    }
    if (state->inst->flags & NACL_IFLAG(OpcodeInModRm)) {
      gprintf(gout, " / %d", modrm_opcode(state->modrm));
      count += 4;
    } else if (state->inst->flags & NACL_IFLAG(OpcodePlusR)) {
      gprintf(gout, " - r%d",
              NaClGetOpcodePlusR(state->inst->opcode_ext));
      count += 5;
    }
    if (state->inst->flags & NACL_IFLAG(OpcodeInModRmRm)) {
      gprintf(gout, " / %d", modrm_rm(state->modrm));
      count += 4;
    }
    /* Add opcode for 0f0f instructions, where the opcode is the last
     * byte of the instruction.
     */
    if ((state->num_opcode_bytes >= 2) &&
        (0 == (state->inst->flags & NACL_IFLAG(Opcode0F0F))) &&
        (0x0F == state->bytes.byte[state->num_prefix_bytes]) &&
        (0x0F == state->bytes.byte[state->num_prefix_bytes + 1])) {
      gprintf(gout, " %02x", state->bytes.byte[state->bytes.length - 1]);
      count += 3;
    }
  }
  while (count < 30) {
    gprintf(gout, " ");
    ++count;
  }
}

void NaClDisassembleSegment(uint8_t* mbase, NaClPcAddress vbase,
                            NaClMemorySize size) {
  if (NACL_FLAGS_use_iter) {
    NaClSegment segment;
    NaClInstIter* iter;
    struct Gio* gout = NaClLogGetGio();
    NaClSegmentInitialize(mbase, vbase, size, &segment);
    iter = NaClInstIterCreate(&segment); NaClInstIterHasNext(iter);
    if (NULL == iter) {
      gprintf(NaClLogGetGio(), "Error: not enough memory\n");
    } else {
      for (; NaClInstIterHasNext(iter); NaClInstIterAdvance(iter)) {
        NaClInstState* state = NaClInstIterGetState(iter);
        NaClInstStateInstPrint(gout, state);
        if (NACL_FLAGS_internal) {
          NaClInstPrintOpcodeSeq(gout, state);
          NaClInstPrint(gout, NaClInstStateInst(state));
          NaClExpVectorPrint(gout, NaClInstStateExpVector(state));
        }
      }
      NaClInstIterDestroy(iter);
    }
  } else {
    NCDecodeSegment(mbase, vbase, size);
  }
}

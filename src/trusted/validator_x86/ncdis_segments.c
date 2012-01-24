/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef NACL_TRUSTED_BUT_NOT_TCB
#error("This file is not meant for use in the TCB")
#endif

#include "native_client/src/trusted/validator_x86/ncdis_segments.h"

#include <string.h>

#include "native_client/src/shared/platform/nacl_log.h"
#include "native_client/src/trusted/validator/x86/decoder/nc_inst_iter.h"
#include "native_client/src/trusted/validator/x86/decoder/nc_inst_state_internal.h"
#include "native_client/src/trusted/validator/x86/decoder/ncop_exps.h"
#include "native_client/src/trusted/validator_x86/ncdis_decode_tables.h"
#include "native_client/src/trusted/validator/x86/ncval_reg_sfi/ncval_decode_tables.h"
#include "native_client/src/trusted/validator/x86/ncval_seg_sfi/ncdecode_verbose.h"
#include "native_client/src/trusted/validator/x86/nc_segment.h"

/* Returns true if the disassemble flag is in the given flag set. */
Bool NaClContainsDisasembleFlag(NaClDisassembleFlags flags,
                                NaClDisassembleFlag flag) {
  return NaClHasBit(flags, NACL_DISASSEMBLE_FLAG(flag)) ? TRUE : FALSE;
}

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

/* Disassemble the code segment, using the given decoder tables.
 * Note: The decoder tables specified in the flags argument will
 * be ignored.
 *
 * Parameters:
 *    mbase - Memory region containing code segment.
 *    vbase - PC address associated with first byte of memory region.
 *    size - Number of bytes in memory region.
 *    flags - Flags to use when decoding.
 */
static void NaClDisassembleSegmentUsingTables(
    uint8_t* mbase, NaClPcAddress vbase,
    NaClMemorySize size, NaClDisassembleFlags flags,
    const struct NaClDecodeTables* decoder_tables)  {
  NaClSegment segment;
  NaClInstIter* iter;
  struct Gio* gout = NaClLogGetGio();
  Bool print_internals =
      NaClHasBit(flags, NACL_DISASSEMBLE_FLAG(NaClDisassembleAddInternals));
  NaClSegmentInitialize(mbase, vbase, size, &segment);
  iter = NaClInstIterCreate(decoder_tables, &segment);
  if (NULL == iter) {
    gprintf(gout, "Error: not enough memory\n");
  } else {
    for (; NaClInstIterHasNext(iter); NaClInstIterAdvance(iter)) {
      NaClInstState* state = NaClInstIterGetState(iter);
      NaClInstStateInstPrint(gout, state);
      if (print_internals) {
        NaClInstPrintOpcodeSeq(gout, state);
        NaClInstPrint(gout, state->decoder_tables, NaClInstStateInst(state));
        NaClExpVectorPrint(gout, state);
      }
    }
    NaClInstIterDestroy(iter);
  }
}

void NaClDisassembleSegment(uint8_t* mbase, NaClPcAddress vbase,
                            NaClMemorySize size, NaClDisassembleFlags flags) {
  if (NaClHasBit(flags, NACL_DISASSEMBLE_FLAG(NaClDisassembleFull))) {
    if (NaClHasBit(flags,
                   NACL_DISASSEMBLE_FLAG(NaClDisassembleValidatorDecoder))) {
      gprintf(NaClLogGetGio(),
              "Error: can't specify both full and validator disassembly,\n"
              "       assuming full disassembly.\n");
    }
    NaClDisassembleSegmentUsingTables(mbase, vbase, size, flags,
                                      kNaClDecoderTables);
  } else if (NaClHasBit
             (flags,
              NACL_DISASSEMBLE_FLAG(NaClDisassembleValidatorDecoder))) {
    if (64 == NACL_TARGET_SUBARCH) {
      NaClDisassembleSegmentUsingTables(mbase, vbase, size, flags,
                                        kNaClValDecoderTables);
    } else {
      NCDecodeSegment(mbase, vbase, size);
    }
  } else {
    gprintf(NaClLogGetGio(),
            "Error: No decoder tables specified, can't disassemble\n");
  }
}

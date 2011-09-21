/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Descriptors to model instructions, opcodes, and instruction operands. */

#include "native_client/src/trusted/validator/x86/decoder/ncopcode_desc.h"

#include <assert.h>
#include <string.h>

#include "native_client/src/trusted/validator/x86/decoder/nc_decode_tables.h"

#include "native_client/src/trusted/validator/x86/decoder/ncopcode_desc_inl.c"

void NaClInstPrint(struct Gio* f,
                   const NaClDecodeTables* tables,
                   const NaClInst* inst) {
  { /* Print out instruction type less the NACLi_ prefix. */
    const char* name = NaClInstTypeString(inst->insttype);
    gprintf(f, "%s ", name + strlen("NACLi_"));
  }
  if (inst->flags) NaClIFlagsPrint(f, inst->flags);
  gprintf(f, "\n");

  /* If instruction type is invalid, and doesn't have
   * special translation purposes, then don't print additional
   * (ignored) information stored in the modeled instruction.
   */
  if ((NACLi_INVALID != inst->insttype) ||
      ((inst->flags & NACL_IFLAG(Opcode0F0F)))) {
    Bool is_first = TRUE;
    int i;
    gprintf(f, "    %s", NaClMnemonicName(inst->name));
    for (i = 0; i < inst->num_operands; ++i) {
      const NaClOp* op = NaClGetInstOperandInline(tables, inst, i);
      if (NULL == op->format_string) continue;
      if (is_first) {
        is_first = FALSE;
      } else {
        gprintf(f, ",");
      }
      gprintf(f, " %s", op->format_string);
    }
    gprintf(f, "\n");
    /* Now print actual encoding of each operand. */
    for (i = 0; i < inst->num_operands; ++i) {
      gprintf(f, "      ");
      NaClOpPrint(f, NaClGetInstOperand(tables, inst, i));
    }
  }
}

/* Dummy routine to allow unreferenced NaClGetInstNumberOperandsInline
 * inline.
 */
uint8_t NaClNcopcodeDescVerboseDummyNaClGetInstNumberOperands(
    const NaClInst* inst) {
  return NaClGetInstNumberOperandsInline(inst);
}

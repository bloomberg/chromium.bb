/*
 * Copyright 2009 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the <LICENSE file.
 */

/* Descriptors to model instructions, opcodes, and instruction operands. */

#include <stdio.h>
#include <assert.h>

#define NEEDSNACLINSTTYPESTRING
#include "native_client/src/trusted/validator_x86/ncopcode_desc.h"
#include "native_client/src/shared/utils/types.h"
#include "gen/native_client/src/trusted/validator_x86/nacl_disallows_impl.h"
#include "gen/native_client/src/trusted/validator_x86/ncopcode_prefix_impl.h"
#include "gen/native_client/src/trusted/validator_x86/ncopcode_insts_impl.h"
#include "gen/native_client/src/trusted/validator_x86/ncopcode_opcode_flags_impl.h"
#include "gen/native_client/src/trusted/validator_x86/ncopcode_operand_kind_impl.h"
#include "gen/native_client/src/trusted/validator_x86/ncopcode_operand_flag_impl.h"

uint8_t NaClGetInstNumberOperands(NaClInst* inst) {
  uint8_t operands = inst->num_operands;
  if (operands > 0 &&
      (inst->operands[0].flags & NACL_OPFLAG(OperandExtendsOpcode))) {
    --operands;
  }
  return operands;
}

NaClOp* NaClGetInstOperand(NaClInst* inst, uint8_t index) {
  if (inst->num_operands > 0 &&
      (inst->operands[0].flags & NACL_OPFLAG(OperandExtendsOpcode))) {
    ++index;
  }
  assert(index < inst->num_operands);
  return &inst->operands[index];
}

void NaClOpFlagsPrint(struct Gio* f, NaClOpFlags flags) {
  if (flags) {
    NaClOpFlag i;
    Bool first = TRUE;
    for (i = 0; i < NaClOpFlagEnumSize; ++i) {
      if (flags & NACL_OPFLAG(i)) {
        if (first) {
          first = FALSE;
        } else {
          gprintf(f, " | ");
        }
        gprintf(f, "NACL_OPFLAG(%s)", NaClOpFlagName(i));
      }
    }
  } else {
    gprintf(f, "NACL_EMPTY_OPFLAGS");
  }
}

void NaClOpPrint(struct Gio* f, NaClOp* operand) {
  gprintf(f, "{ %s, ", NaClOpKindName(operand->kind));
  NaClOpFlagsPrint(f, operand->flags);
  gprintf(f, " },\n");
}

void NaClIFlagsPrint(struct Gio* f, NaClIFlags flags) {
  if (flags) {
    int i;
    Bool first = TRUE;
    for (i = 0; i < NaClIFlagEnumSize; ++i) {
      if (flags & NACL_IFLAG(i)) {
        if (first) {
          first = FALSE;
        } else {
          gprintf(f, " | ");
        }
        gprintf(f, "NACL_IFLAG(%s)", NaClIFlagName(i));
      }
    }
  } else {
    gprintf(f, "NACL_EMPTY_IFLAGS");
  }
}

void NaClInstPrintTableDriver(struct Gio* f, Bool as_array_element,
                              Bool simplify, int index,
                              NaClInst* inst, int lookahead) {
  int i;
  /* Note that if we are to simplify, we don't bother to print
   * out non-useful information, and slightly reorder the fields
   * so that the instruction mnemonic is easier to read.
   */
  if (!simplify && index >= 0) {
    gprintf(f, "  /* %d */\n", index);
  }

  gprintf(f, "  { %s,", NaClInstPrefixName(inst->prefix));
  if (!simplify) {
    gprintf(f, " %"NACL_PRIu8",", inst->num_opcode_bytes);
  }
  gprintf(f, " { ");
  for (i = 0; i < NACL_MAX_OPCODE_BYTES; ++i) {
    if (!simplify || i < inst->num_opcode_bytes) {
      if (i > 0) gprintf(f, ",");
      gprintf(f," 0x%02x", inst->opcode[i]);
    }
  }
  gprintf(f, " },");
  if (simplify) {
    gprintf(f, " %s, ", NaClMnemonicName(inst->name));
  }
  gprintf(f, " %s,\n", kNaClInstTypeString[inst->insttype]);
  gprintf(f, "    ");
  NaClIFlagsPrint(f, inst->flags);
  gprintf(f, ",\n");
  if (!simplify) {
    gprintf(f, "    Inst%s,\n", NaClMnemonicName(inst->name));
  }
  if (!simplify) {
    gprintf(f, "    %u, {\n", inst->num_operands);
  }
  for (i = 0; i < NACL_MAX_NUM_OPERANDS; ++i) {
    if (!simplify || i < inst->num_operands) {
      if (!simplify) gprintf(f, "  ");
      gprintf(f, "    ");
      NaClOpPrint(f, inst->operands + i);
    }
  }
  if (simplify) {
    gprintf(f, "  };\n");
  } else {
    gprintf(f, "    },\n");
    if (index < 0 || NULL == inst->next_rule) {
      gprintf(f, "    NULL\n");
    } else {
      gprintf(f, "    g_Opcodes + %d\n", lookahead);
    }
    if (as_array_element) {
      gprintf(f, "  }%s\n", index < 0 ? ";" : ",");
    } else {
      gprintf(f, "  };\n");
    }
  }
}

void NaClInstPrintTablegen(struct Gio* f, int index, NaClInst* inst,
                           int lookahead) {
  NaClInstPrintTableDriver(f, TRUE, FALSE, index, inst, lookahead);
}

void NaClInstPrint(struct Gio* f, NaClInst* inst) {
  NaClInstPrintTableDriver(f, FALSE, TRUE, -1, inst, 0);
}

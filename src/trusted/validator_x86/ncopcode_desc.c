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

void NaClOpFlagsPrint(FILE* f, NaClOpFlags flags) {
  if (flags) {
    NaClOpFlag i;
    Bool first = TRUE;
    for (i = 0; i < NaClOpFlagEnumSize; ++i) {
      if (flags & NACL_OPFLAG(i)) {
        if (first) {
          first = FALSE;
        } else {
          fprintf(f, " | ");
        }
        fprintf(f, "NACL_OPFLAG(%s)", NaClOpFlagName(i));
      }
    }
  } else {
    fprintf(f, "NACL_EMPTY_OPFLAGS");
  }
}

void NaClOpPrint(FILE* f, NaClOp* operand) {
  fprintf(f, "      { %s, ", NaClOpKindName(operand->kind));
  NaClOpFlagsPrint(f, operand->flags);
  fprintf(f, " },\n");
}

void NaClIFlagsPrint(FILE* f, NaClIFlags flags) {
  if (flags) {
    int i;
    Bool first = TRUE;
    for (i = 0; i < NaClIFlagEnumSize; ++i) {
      if (flags & NACL_IFLAG(i)) {
        if (first) {
          first = FALSE;
        } else {
          fprintf(f, " | ");
        }
        fprintf(f, "NACL_IFLAG(%s)", NaClIFlagName(i));
      }
    }
  } else {
    fprintf(f, "NACL_EMPTY_IFLAGS");
  }
}

void NaClInstPrintTableDriver(FILE* f, Bool as_array_element,
                              Bool simplify, int index,
                              NaClInst* inst, int lookahead) {
  int i;
  if (!simplify && index >= 0) {
    fprintf(f, "  /* %d */\n", index);
  }

  fprintf(f, "  { %"NACL_PRIu8", {", inst->num_opcode_bytes);
  for (i = 0; i < NACL_MAX_OPCODE_BYTES; ++i) {
    if (i > 0) fprintf(f, ",");
    fprintf(f," 0x%02x", inst->opcode[i]);
  }
  fprintf(f, " },\n");
  fprintf(f, "    %s,\n", kNaClInstTypeString[inst->insttype]);
  fprintf(f, "    ");
  NaClIFlagsPrint(f, inst->flags);
  fprintf(f, ",\n");
  fprintf(f, "    Inst%s,\n", NaClMnemonicName(inst->name));
  fprintf(f, "    %u, {\n", inst->num_operands);
  for (i = 0; i < NACL_MAX_NUM_OPERANDS; ++i) {
    if (!simplify || i < inst->num_operands) {
      NaClOpPrint(f, inst->operands + i);
    }
  }
  if (simplify) {
    fprintf(f, "  } };\n");
  } else {
    fprintf(f, "    },\n");
    if (index < 0 || NULL == inst->next_rule) {
      fprintf(f, "    NULL\n");
    } else {
      fprintf(f, "    g_Opcodes + %d\n", lookahead);
    }
    if (as_array_element) {
      fprintf(f, "  }%s\n", index < 0 ? ";" : ",");
    } else {
      fprintf(f, "  };\n");
    }
  }
}

void NaClInstPrintTablegen(FILE* f, int index, NaClInst* inst, int lookahead) {
  NaClInstPrintTableDriver(f, TRUE, FALSE, index, inst, lookahead);
}

void NaClInstPrint(FILE* f, NaClInst* inst) {
  NaClInstPrintTableDriver(f, FALSE, TRUE, -1, inst, 0);
}

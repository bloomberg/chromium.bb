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

/* Descriptors to model instructions, opcodes, and instruction operands. */

#include <stdio.h>
#include <assert.h>

#define NEEDSNACLINSTTYPESTRING
#include "native_client/src/trusted/validator_x86/ncopcode_desc.h"
#include "native_client/src/shared/utils/types.h"
#include "gen/native_client/src/trusted/validator_x86/ncopcode_prefix_impl.h"
#include "gen/native_client/src/trusted/validator_x86/ncopcode_insts_impl.h"
#include "gen/native_client/src/trusted/validator_x86/ncopcode_opcode_flags_impl.h"
#include "gen/native_client/src/trusted/validator_x86/ncopcode_operand_kind_impl.h"
#include "gen/native_client/src/trusted/validator_x86/ncopcode_operand_flag_impl.h"

uint8_t NcGetOpcodeNumberOperands(Opcode* opcode) {
  uint8_t operands = opcode->num_operands;
  if (operands > 0 &&
      (opcode->operands[0].flags & OpFlag(OperandExtendsOpcode))) {
    --operands;
  }
  return operands;
}

Operand* NcGetOpcodeOperand(Opcode* opcode, uint8_t index) {
  if (opcode->num_operands > 0 &&
      (opcode->operands[0].flags & OpFlag(OperandExtendsOpcode))) {
    ++index;
  }
  assert(index < opcode->num_operands);
  return &opcode->operands[index];
}

void PrintOperand(FILE* f, Operand* operand) {
  fprintf(f, "      { %s, ", OperandKindName(operand->kind));
  if (operand->flags) {
    OperandFlag i;
    Bool first = TRUE;
    for (i = 0; i < OperandFlagEnumSize; ++i) {
      if (operand->flags & OpFlag(i)) {
        if (first) {
          first = FALSE;
        } else {
          fprintf(f, " | ");
        }
        fprintf(f, "OpFlag(%s)", OperandFlagName(i));
      }
    }
  } else {
    fprintf(f, "0");
  }
  fprintf(f, " },\n");
}

/* Prints out the given opcode structure to the given file. If index >= 0,
 * print out a comment, with the value of index, before the printed opcode
 * structure. Lookahead is used to convert the next_rule pointer into
 * a symbolic reference using the name "g_Opcodes", plus the index defined by
 * the lookahead. Argument as_array_element is true if the element is
 * assumed to be in an array static initializer. If argument simplify is
 * true, then the element is for documentation purposes only (as a single
 * element), and simplify the output to only contain (user-readable)
 * useful information.
 */
void PrintOpcodeTableDriver(FILE* f, Bool as_array_element,
                            Bool simplify, int index,
                            Opcode* opcode, int lookahead) {
  int i;
  if (!simplify && index >= 0) {
    fprintf(f, "  /* %d */\n", index);
  }
  fprintf(f, "  { %"PRIu8", {", opcode->num_opcode_bytes);
  for (i = 0; i < MAX_OPCODE_BYTES; ++i) {
    if (i > 0) fprintf(f, ",");
    fprintf(f," 0x%02x", opcode->opcode[i]);
  }
  fprintf(f, " },\n");
  fprintf(f, "    %s,\n", kNaClInstTypeString[opcode->insttype]);
  fprintf(f, "    ");
  if (opcode->flags) {
    Bool first = TRUE;
    for (i = 0; i < OpcodeFlagEnumSize; ++i) {
      if (opcode->flags & InstFlag(i)) {
        if (first) {
          first = FALSE;
        } else {
          fprintf(f, " | ");
        }
        fprintf(f, "InstFlag(%s)", OpcodeFlagName(i));
      }
    }
  } else {
    fprintf(f, "0");
  }
  fprintf(f, ",\n");
  fprintf(f, "    Inst%s,\n", InstMnemonicName(opcode->name));
  fprintf(f, "    %u, {\n", opcode->num_operands);
  for (i = 0; i < MAX_NUM_OPERANDS; ++i) {
    if (!simplify || i < opcode->num_operands) {
      PrintOperand(f, opcode->operands + i);
    }
  }
  if (simplify) {
    fprintf(f, "  } };\n");
  } else {
    fprintf(f, "    },\n");
    if (index < 0 || NULL == opcode->next_rule) {
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

void PrintOpcodeTablegen(FILE* f, int index, Opcode* opcode, int lookahead) {
  PrintOpcodeTableDriver(f, TRUE, FALSE, index, opcode, lookahead);
}

void PrintOpcode(FILE* f, Opcode* opcode) {
  PrintOpcodeTableDriver(f, FALSE, TRUE, -1, opcode, 0);
}

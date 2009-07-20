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

#define NEEDSNACLINSTTYPESTRING
#include "native_client/src/trusted/validator_x86/ncopcode_desc.h"
#include "native_client/src/shared/utils/types.h"

/* Define the corresponding names of OpcodeFlag's. */
static const char* const g_OpcodeFlagName[OpcodeFlagEnumSize] = {
  "OpcodeUsesRexW",
  "OpcodeHasRexR",
  "OpcodeHasNoRexR",
  "OpcodeInModRm",
  "OpcodeUsesModRm",
  "OpcodeHasImmed",
  "OpcodeHasImmed_b",
  "OpcodeHasImmed_w",
  "OpcodeHasImmed_v",
  "OpcodePlusR",
  "OpcodePlusI",
  "OpcodeRex",
  "OpcodeLegacy",
  "OpcodeLockable",
  "Opcode32Only",
  "Opcode64Only",
  "OperandSize_b",
  "OperandSize_w",
  "OperandSize_v",
  "OperandSize_o",
  "AddressSize_w",
  "AddressSize_v",
  "AddressSize_o",
  "OperandSizeDefaultIs64",
  "OperandSizeForce64",
  /* "OpcodeOF" */
};

const char* OpcodeFlagName(const OpcodeFlagEnum flag) {
  return flag < OpcodeFlagEnumSize ? g_OpcodeFlagName[flag] : "OpcodeFlag???";
}

/* Define the corresponding names of OperandKind. */
static const char* const g_OperandKindName[OperandKindEnumSize] = {
  "Unknown_Operand",
  "A_Operand",
  "Aw_Operand",
  "Av_Operand",
  "Ao_Operand",
  "E_Operand",
  "Eb_Operand",
  "Ew_Operand",
  "Ev_Operand",
  "Eo_Operand",
  "G_Operand",
  "Gb_Operand",
  "Gw_Operand",
  "Gv_Operand",
  "Go_Operand",
  "G_OpcodeBase",
  "I_Operand",
  "Ib_Operand",
  "Iw_Operand",
  "Iv_Operand",
  "J_Operand",
  "Jb_Operand",
  "Jw_Operand",
  "Jv_Operand",
  "M_Operand",
  "Mb_Operand",
  "Mw_Operand",
  "Mv_Operand",
  "Mo_Operand",
  "Mqo_Operand",
  "Mpw_Operand",
  "Mpv_Operand",
  "Mpo_Operand",
  "O_Operand",
  "Ob_Operand",
  "Ow_Operand",
  "Ov_Opernd",
  "Oo_Operand",
  "S_Operand",
  "RegUnknown",
  "RegAL",
  "RegBL",
  "RegCL",
  "RegDL",
  "RegAH",
  "RegBH",
  "RegCH",
  "RegDH",
  "RegDIL",
  "RegSIL",
  "RegBPL",
  "RegSPL",
  "RegR8L",
  "RegR9L",
  "RegR10L",
  "RegR11L",
  "RegR12L",
  "RegR13L",
  "RegR14L",
  "RegR15L",
  "RegAX",
  "RegBX",
  "RegCX",
  "RegDX",
  "RegSI",
  "RegDI",
  "RegBP",
  "RegSP",
  "RegR8W",
  "RegR9W",
  "RegR10W",
  "RegR11W",
  "RegR12W",
  "RegR13W",
  "RegR14W",
  "RegR15W",
  "RegEAX",
  "RegEBX",
  "RegECX",
  "RegEDX",
  "RegESI",
  "RegEDI",
  "RegEBP",
  "RegESP",
  "RegR8D",
  "RegR9D",
  "RegR10D",
  "RegR11D",
  "RegR12D",
  "RegR13D",
  "RegR14D",
  "RegR15D",
  "RegCS",
  "RegDS",
  "RegSS",
  "RegES",
  "RegFS",
  "RegGS",
  "RegEFLAGS",
  "RegRFLAGS",
  "RegEIP",
  "RegRIP",
  "RegRAX",
  "RegRBX",
  "RegRCX",
  "RegRDX",
  "RegRSI",
  "RegRDI",
  "RegRBP",
  "RegRSP",
  "RegR8",
  "RegR9",
  "RegR10",
  "RegR11",
  "RegR12",
  "RegR13",
  "RegR14",
  "RegR15",
  "RegRESP",
  "RegREAX",
  "RegREIP",
  "RegREBP",
  "RegGP7",
  "OpcodeBaseMinus0",
  "OpcodeBaseMinus1",
  "OpcodeBaseMinus2",
  "OpcodeBaseMinus3",
  "OpcodeBaseMinus4",
  "OpcodeBaseMinus5",
  "OpcodeBaseMinus6",
  "OpcodeBaseMinus7",
  "Opcode0",
  "Opcode1",
  "Opcode2",
  "Opcode3",
  "Opcode4",
  "Opcode5",
  "Opcode6",
  "Opcode7"
};

const char* OperandKindName(const OperandKind kind) {
  return kind < OperandKindEnumSize
      ? g_OperandKindName[kind]
      : "OperandKind???";
}

/* Define the corresponding names of OperandFlag. */
static const char* const g_OperandFlagName[OperandFlagEnumSize] = {
  "RexExcludesAhBhChDh",
  "OpUse",
  "OpSet",
  "OpImplicit",
  "OperandExtendsOpcode",
  "OperandNear",
  "OperandRelative",
  "OperandUsesAddressSize",
};

const char* OperandFlagName(const OperandFlagEnum flag) {
  return flag < OperandFlagEnumSize
      ? g_OperandFlagName[flag]
      : "OperandFlag???";
}

/* Define the corresponding names of InstMnemonic. */
static const char* const g_InstMnemonicName[InstMnemonicEnumSize] = {
  "Undefined",
  "Aaa",
  "Aas",
  "Adc",
  "Add",
  "And",
  "Arpl",
  "Bound",
  "Call",
  "Cmp",
  "Daa",
  "Das",
  "Dec",
  "Imul",
  "Hlt",
  "Inc",
  "Jb",
  "Jbe",
  "Jcxz",
  "Jg",
  "Jge",
  "Jl",
  "Jle",
  "Jmp",
  "Jnb",
  "Jnbe",
  "Jno",
  "Jnp",
  "Jns",
  "Jnz",
  "Jo",
  "Jp",
  "Js",
  "Jz",
  "Leave",
  "Mov",
  "Movsxd",
  "Nop",
  "Or",
  "Pop",
  "Popa",
  "Popad",
  "Push",
  "Pusha",
  "Pushad",
  "Ret",
  "Sbb",
  "Sub",
  "Test",
  "Xor",
};

const char* InstMnemonicName(const InstMnemonic mnemonic) {
  return mnemonic < InstMnemonicEnumSize
      ? g_InstMnemonicName[mnemonic]
      : "InstMnemonic???";
}

/* Define the corresponding names of OpcodePrefix. */
static const char* const g_OpcodePrefixName[OpcodePrefixEnumSize] = {
  "NoPrefix",
  "Prefix0F",
  "PrefixF20F",
  "PrefixF30F",
  "Prefix660F",
  "Prefix0F0F",
  "Prefix0F38",
  "Prefix660F38",
  "PrefixF20F38",
  "Prefix0F3A",
  "Prefix660F3A"
};

const char* OpcodePrefixName(OpcodePrefix prefix) {
  return prefix < OpcodePrefixEnumSize
      ? g_OpcodePrefixName[prefix]
      : "OpcodePrefix???";
}

void PrintOperand(FILE* f, Operand* operand) {
  fprintf(f, "      { %s, ", OperandKindName(operand->kind));
  if (operand->flags) {
    OperandFlagEnum i;
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
  fprintf(f, "  { 0x%02x,\n", opcode->opcode);
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

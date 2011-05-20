/*
 * Copyright 2009 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

/* Descriptors to model instructions, opcodes, and instruction operands. */

#include <assert.h>
#include <string.h>

#define NEEDSNACLINSTTYPESTRING
#include "native_client/src/trusted/validator_x86/ncopcode_desc.h"
#include "native_client/src/shared/utils/types.h"

/* Generated files */
#include "native_client/src/trusted/validator_x86/gen/nacl_disallows_impl.h"
#include "native_client/src/trusted/validator_x86/gen/ncopcode_prefix_impl.h"
#include "native_client/src/trusted/validator_x86/gen/ncopcode_insts_impl.h"
#include "native_client/src/trusted/validator_x86/gen/ncopcode_opcode_flags_impl.h"
#include "native_client/src/trusted/validator_x86/gen/ncopcode_operand_kind_impl.h"
#include "native_client/src/trusted/validator_x86/gen/ncopcode_operand_flag_impl.h"

uint8_t NaClGetInstNumberOperands(const NaClInst* inst) {
  return inst->num_operands;
}

const NaClOp* NaClGetInstOperand(const NaClInst* inst, uint8_t index) {
  assert(index < inst->num_operands);
  return &inst->operands[index];
}

/* Print out the opcode operand flags in a simplified (i.e. more human readable)
 * form.
 */
void NaClOpFlagsPrint(struct Gio* f, NaClOpFlags flags) {
  NaClOpFlag i;
  Bool first = TRUE;
  for (i = 0; i < NaClOpFlagEnumSize; ++i) {
    if (flags & NACL_OPFLAG(i)) {
      if (first) {
        first = FALSE;
      } else {
        gprintf(f, " ");
      }
      gprintf(f, "%s", NaClOpFlagName(i));
    }
  }
}

/* Print out the opcode operand in a simplified (i.e. more human readable)
 * form.
 */
void NaClOpPrint(struct Gio* f, const NaClOp* operand) {
  gprintf(f, "%s", NaClOpKindName(operand->kind));
  if (operand->flags) {
    size_t i;
    for (i = strlen(NaClOpKindName(operand->kind)); i < 24; ++i) {
      gprintf(f, " ");
    }
    NaClOpFlagsPrint(f, operand->flags);
  }
  gprintf(f, "\n");
}

/* Print instruction flags using a simplified (i.e. more human readable) form */
void NaClIFlagsPrint(struct Gio* f, NaClIFlags flags) {
  int i;
  Bool first = TRUE;
  for (i = 0; i < NaClIFlagEnumSize; ++i) {
    if (flags & NACL_IFLAG(i)) {
      if (first) {
        first = FALSE;
      } else {
        gprintf(f, " ");
      }
      gprintf(f, "%s", NaClIFlagName(i));
    }
  }
}

/* Returns a string defining bytes of the given prefix that are considered
 * prefix bytes, independent of the opcode.
 */
static const char* OpcodePrefixBytes(NaClInstPrefix prefix) {
  switch(prefix) {
    case NoPrefix:
    case Prefix0F:
    case Prefix0F0F:
    case Prefix0F38:
    case Prefix0F3A:
    case PrefixD8:
    case PrefixD9:
    case PrefixDA:
    case PrefixDB:
    case PrefixDC:
    case PrefixDD:
    case PrefixDE:
    case PrefixDF:
    default:  /* This shouldn't happen, but be safe. */
      return "";
    case PrefixF20F:
    case PrefixF20F38:
      return "f2";
    case PrefixF30F:
      return "f3";
    case Prefix660F:
    case Prefix660F38:
    case Prefix660F3A:
      return "66";
  }
}

void NaClInstPrint(struct Gio* f, const NaClInst* inst) {
  int i;
  int count = 2;
  /* Add prefix bytes if defined by prefix. */
  const char* prefix = OpcodePrefixBytes(inst->prefix);
  int prefix_len = (int) strlen(prefix);
  gprintf(f, "  %s", prefix);
  if (prefix_len > 0) {
    count += prefix_len + 1;
    gprintf(f, " ");
  }

  /* Add opcode bytes. */
  for (i = 0; i < inst->num_opcode_bytes; ++i) {
    if (i > 0) {
      gprintf(f, " ");
      ++count;
    }
    gprintf(f,"%02x", inst->opcode[i]);
    count += 2;
  }
  if (inst->flags & NACL_IFLAG(OpcodeInModRm)) {
    gprintf(f, " / %d", inst->opcode[inst->num_opcode_bytes]);
    count += 4;
  } else if (inst->flags & NACL_IFLAG(OpcodePlusR)) {
    gprintf(f, " - r%d", inst->opcode[inst->num_opcode_bytes]);
    count += 5;
  }
  if (inst->flags & NACL_IFLAG(OpcodeInModRmRm)) {
    gprintf(f, " / %d", inst->opcode[inst->num_opcode_bytes + 1]);
    count += 4;
  }
  while (count < 30) {
    gprintf(f, " ");
    ++count;
  }
  { /* Print out instruction type less the NACLi_ prefix. */
    const char* name = kNaClInstTypeString[inst->insttype];
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
      if (NULL == inst->operands[i].format_string) continue;
      if (is_first) {
        is_first = FALSE;
      } else {
        gprintf(f, ",");
      }
      gprintf(f, " %s", inst->operands[i].format_string);
    }
    gprintf(f, "\n");
    /* Now print actual encoding of each operand. */
    for (i = 0; i < inst->num_operands; ++i) {
      gprintf(f, "      ");
      NaClOpPrint(f, inst->operands + i);
    }
  }
}

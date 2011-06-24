/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#define NEEDSNACLINSTTYPESTRING

#include "native_client/src/trusted/validator_x86/modeled_nacl_inst.h"

#include <string.h>

void NaClModeledInstPrint(struct Gio* f, const NaClModeledInst* inst) {
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

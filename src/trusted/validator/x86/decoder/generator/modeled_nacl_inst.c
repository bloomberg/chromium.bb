/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#define NEEDSNACLINSTTYPESTRING

#include "native_client/src/trusted/validator/x86/decoder/generator/modeled_nacl_inst.h"

#include <string.h>

void NaClSetOpcodeInModRm(uint8_t value, uint8_t *opcode_ext) {
  *opcode_ext += value;
}

void NaClSetOpcodeInModRmRm(uint8_t value, uint8_t *opcode_ext) {
  *opcode_ext += (value << 4);
}

void NaClSetOpcodePlusR(uint8_t value, uint8_t *opcode_ext) {
  *opcode_ext += value;
}

/* Print the flag name if the flag is defined for the corresponding operand.
 * Used to print out set/use/zero extend information for partial instructions.
 */
static void NaClPrintAddOperandFlag(struct Gio* f,
                                    const NaClOp* op,
                                    NaClOpFlag flag,
                                    const char* flag_name) {
  if (op->flags & NACL_OPFLAG(flag)) {
    gprintf(f, "%s", flag_name);
  }
}

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
    gprintf(f, " / %d", NaClGetOpcodeInModRm(inst->opcode_ext));
    count += 4;
  } else if (inst->flags & NACL_IFLAG(OpcodePlusR)) {
    gprintf(f, " - r%d", NaClGetOpcodePlusR(inst->opcode_ext));
    count += 5;
  }
  if (inst->flags & NACL_IFLAG(OpcodeInModRmRm)) {
    gprintf(f, " / %d", NaClGetOpcodeInModRmRm(inst->opcode_ext));
    count += 4;
  }
  while (count < 30) {
    gprintf(f, " ");
    ++count;
  }
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
    gprintf(f, "    ");

    /* Instruction has been simplified. Print out corresponding
     * hints to the reader, so that they know that the instruction
     * has been simplified.
     */
    if (NaClHasBit(inst->flags, NACL_IFLAG(PartialInstruction))) {
      gprintf(f, "[P] ");
    }
    gprintf(f, "%s", NaClMnemonicName(inst->name));

    /* If an instruction has been simplified, and it illegal, communicate
     * that in the printed modeled instruction.
     */
    if (NaClHasBit(inst->flags, NACL_IFLAG(NaClIllegal)) &&
        NaClHasBit(inst->flags, NACL_IFLAG(PartialInstruction))) {
      gprintf(f, "(illegal)");
    }
    for (i = 0; i < inst->num_operands; ++i) {
      if (NULL == inst->operands[i].format_string) continue;
      if (is_first) {
        is_first = FALSE;
      } else {
        gprintf(f, ",");
      }
      gprintf(f, " %s", inst->operands[i].format_string);

      /* If this is a partial instruction, add set/use information
       * so that that it is more clear what was matched.
       */
      if (NaClHasBit(inst->flags, NACL_IFLAG(PartialInstruction))) {
        const NaClOp* op = inst->operands + i;
        if (NaClHasBit(op->flags, (NACL_OPFLAG(OpSet) |
                                   NACL_OPFLAG(OpUse) |
                                   NACL_OPFLAG(OperandZeroExtends_v)))) {
          gprintf(f, " (");
          NaClPrintAddOperandFlag(f, op, OpSet, "s");
          NaClPrintAddOperandFlag(f, op, OpUse, "u");
          NaClPrintAddOperandFlag(f, op, OperandZeroExtends_v, "z");
          gprintf(f, ")");
        }
      }
    }
    gprintf(f, "\n");
    /* Now print actual encoding of each operand. */
    for (i = 0; i < inst->num_operands; ++i) {
      gprintf(f, "      ");
      NaClOpPrint(f, inst->operands + i);
    }
  }
}

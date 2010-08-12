/*
 * Copyright 2009 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the <LICENSE file.
 */

/* Descriptors to model instructions, opcodes, and instruction operands. */

#include <assert.h>
#include <string.h>

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
void NaClOpPrint(struct Gio* f, NaClOp* operand) {
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

void NaClInstPrint(struct Gio* f, NaClInst* inst) {
  int i;
  int count = 2;
  Bool simplified_opcode_ext = FALSE;
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

  /* If opcode continues in modrm byte, then add the value to this list. */
  if ((0 < inst->num_operands) &&
      (inst->operands[0].flags & NACL_OPFLAG(OperandExtendsOpcode))) {
    if (inst->flags & NACL_IFLAG(OpcodeInModRm)) {
      switch (inst->operands[0].kind) {
        case Opcode0:
        case Opcode1:
        case Opcode2:
        case Opcode3:
        case Opcode4:
        case Opcode5:
        case Opcode6:
        case Opcode7:
          gprintf(f, " / %d", inst->operands[0].kind - Opcode0);
          count += 4;
          simplified_opcode_ext = TRUE;
          break;
        default:
          break;
      }
    } else if (inst->flags & NACL_IFLAG(OpcodePlusR)) {
      switch (inst->operands[0].kind) {
        case OpcodeBaseMinus0:
        case OpcodeBaseMinus1:
        case OpcodeBaseMinus2:
        case OpcodeBaseMinus3:
        case OpcodeBaseMinus4:
        case OpcodeBaseMinus5:
        case OpcodeBaseMinus6:
        case OpcodeBaseMinus7:
          gprintf(f, " - r%d", inst->operands[0].kind - OpcodeBaseMinus0);
          count += 5;
          simplified_opcode_ext = TRUE;
          break;
        default:
          break;
      }
    }
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

  /* If instruction type is invalid, then don't print additional
   * (ignored) information stored in the modeled instruction.
   */
  if (NACLi_INVALID != inst->insttype) {
    Bool is_first = TRUE;
    gprintf(f, "    %s", NaClMnemonicName(inst->name));
    /* First print out print arguments on same line as mnemonic. */
    for (i = 0; i < inst->num_operands; ++i) {
      if (((i == 0) && simplified_opcode_ext) ||
          (NACL_EMPTY_OPFLAGS !=
           (inst->operands[i].flags & (NACL_OPFLAG(OpImplicit) |
                                       NACL_OPFLAG(OperandExtendsOpcode))))) {
      } else {
        NaClOpKind kind = inst->operands[i].kind;
        if (is_first) {
          gprintf(f, " ");
          is_first = FALSE;
        } else {
          gprintf(f, ", ");
        }
        switch(kind) {
          case A_Operand:
          case Aw_Operand:
          case Av_Operand:
          case Ao_Operand:
            gprintf(f, "$A");
            break;
          case E_Operand:
          case Eb_Operand:
          case Ew_Operand:
          case Ev_Operand:
          case Eo_Operand:
          case Edq_Operand:
            gprintf(f, "$E");
            break;
          case G_Operand:
          case Gb_Operand:
          case Gw_Operand:
          case Gv_Operand:
          case Go_Operand:
          case Gdq_Operand:
            gprintf(f, "$G");
            break;
          case ES_G_Operand:
            gprintf(f, "es:$G");
            break;
          case G_OpcodeBase:
            gprintf(f, "$/r");
            break;
          case I_Operand:
          case Ib_Operand:
          case Iw_Operand:
          case Iv_Operand:
          case Io_Operand:
          case I2_Operand:
          case J_Operand:
          case Jb_Operand:
          case Jw_Operand:
          case Jv_Operand:
            gprintf(f, "$I");
            break;
          case M_Operand:
          case Mb_Operand:
          case Mw_Operand:
          case Mv_Operand:
          case Mo_Operand:
          case Mdq_Operand:
            gprintf(f, "$M");
            break;
          case Mpw_Operand:
            gprintf(f, "m16:16");
            break;
          case Mpv_Operand:
            gprintf(f, "m16:32");
            break;
          case Mpo_Operand:
            gprintf(f, "m16:64");
            break;
          case Mmx_E_Operand:
          case Mmx_N_Operand:
            gprintf(f, "$E(mmx)");
            break;
          case Mmx_G_Operand:
          case Mmx_Gd_Operand:
            gprintf(f, "$G(mmx)");
            break;
          case Xmm_E_Operand:
          case Xmm_Eo_Operand:
            gprintf(f, "$E(xmm)");
            break;
          case Xmm_G_Operand:
          case Xmm_Go_Operand:
            gprintf(f, "$G(xmm)");
            break;
          case O_Operand:
          case Ob_Operand:
          case Ow_Operand:
          case Ov_Operand:
          case Oo_Operand:
            gprintf(f, "$O");
            break;
          case S_Operand:
            gprintf(f, "%Sreg");
            break;
          case St_Operand:
            gprintf(f, "%%st");
            break;
          case RegAL:
          case RegBL:
          case RegCL:
          case RegDL:
          case RegAH:
          case RegBH:
          case RegCH:
          case RegDH:
          case RegDIL:
          case RegSIL:
          case RegBPL:
          case RegSPL:
          case RegR8B:
          case RegR9B:
          case RegR10B:
          case RegR11B:
          case RegR12B:
          case RegR13B:
          case RegR14B:
          case RegR15B:
          case RegAX:
          case RegBX:
          case RegCX:
          case RegDX:
          case RegSI:
          case RegDI:
          case RegBP:
          case RegSP:
          case RegR8W:
          case RegR9W:
          case RegR10W:
          case RegR11W:
          case RegR12W:
          case RegR13W:
          case RegR14W:
          case RegR15W:
          case RegEAX:
          case RegEBX:
          case RegECX:
          case RegEDX:
          case RegESI:
          case RegEDI:
          case RegEBP:
          case RegESP:
          case RegR8D:
          case RegR9D:
          case RegR10D:
          case RegR11D:
          case RegR12D:
          case RegR13D:
          case RegR14D:
          case RegR15D:
          case RegCS:
          case RegDS:
          case RegSS:
          case RegES:
          case RegFS:
          case RegGS:
          case RegEFLAGS:
          case RegRFLAGS:
          case RegEIP:
          case RegRIP:
          case RegRAX:
          case RegRBX:
          case RegRCX:
          case RegRDX:
          case RegRSI:
          case RegRDI:
          case RegRBP:
          case RegRSP:
          case RegR8:
          case RegR9:
          case RegR10:
          case RegR11:
          case RegR12:
          case RegR13:
          case RegR14:
          case RegR15:
          case RegRESP:
          case RegREAX:
          case RegREDX:
          case RegREIP:
          case RegREBP:
          case RegDS_EDI:
          case RegES_EDI:
          case RegST0:
          case RegST1:
          case RegST2:
          case RegST3:
          case RegST4:
          case RegST5:
          case RegST6:
          case RegST7:
          case RegMMX0:
          case RegMMX1:
          case RegMMX2:
          case RegMMX3:
          case RegMMX4:
          case RegMMX5:
          case RegMMX6:
          case RegMMX7:
          case RegXMM0:
          case RegXMM1:
          case RegXMM2:
          case RegXMM3:
          case RegXMM4:
          case RegXMM5:
          case RegXMM6:
          case RegXMM7:
          case RegXMM8:
          case RegXMM9:
          case RegXMM10:
          case RegXMM11:
          case RegXMM12:
          case RegXMM13:
          case RegXMM14:
          case RegXMM15:
            gprintf(f, "%%%s", NaClOpKindName(kind) + strlen("Reg"));
            break;
          case Const_1:
            gprintf(f, "1");
            break;
          default:
            gprintf(f, "???");
            break;
        }
      }
    }
    gprintf(f, "\n");
    /* Now print actual encoding of each operand. */
    for (i = 0; i < inst->num_operands; ++i) {
      if ((i == 0) && simplified_opcode_ext) {
        /* Printed out as part of opcode sequence, don't repeat for
         * human readable form.
         */
      } else {
        gprintf(f, "      ");
        NaClOpPrint(f, inst->operands + i);
      }
    }
  }
}

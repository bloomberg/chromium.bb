/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * Simplifies instruction set to what is needed for the
 * (x86-64) ncval_reg_sfi validator.
 *
 * There are two parts to this story. First, the decoder used by the
 * ncval_reg_sfi validator use the decoder in:
 *   native_client/src/trusted/validator/x86/decoder
 *
 * This decoder is based on instruction flags. If the instruction flags
 * are not changed, the same sequence of bytes will be parsed, independent
 * of the instruction mnemonic. Therefore, it is safe to change the
 * instruction mnemonic to a "don't care" name unless it is explicitly
 * used in some rule of the validator. The static validator validator_names
 * in function NaClNcvalSimplifyMnemonic lists all instruction mnemonics that
 * are currently used by the validator.
 *
 * Secondly, the ncval_reg_sfi validator checks general purpose registers,
 * segment registers, and memory references to make sure that data and
 * branching have been sandboxed properly. Hence, any argument to an x86
 * instruction that can be shown that it will never access one of these
 * forms can be omitted without breaking the validator. Hence, this code
 * removes all such arguments. See function NaClNcvalKeepOperand for details
 * on what arguments are removed.
 *
 * Finally, for pragmatic reasons (i.e. readability), if an instruction is
 * modified by this simplification, it is marked as a "partial instruction",
 * so that the corresponding (ncdis) disassembler can correctly communicate
 * that a partial match (rather than an actual instruction decoding) was applied
 * to the given bits. The printed partial match communicates what was kept for
 * validation purposes.
 */

#ifndef NACL_TRUSTED_BUT_NOT_TCB
#error("This file is not meant for use in the TCB")
#endif

#include "native_client/src/trusted/validator/x86/decoder/generator/ncval_simplify.h"

#include <string.h>
#include "native_client/src/include/nacl_macros.h"
#include "native_client/src/trusted/validator/x86/x86_insts.h"
#include "native_client/src/trusted/validator/x86/decoder/generator/nc_compress.h"

/* To turn on debugging of instruction decoding, change value of
 * DEBUGGING to 1.
 */
#define DEBUGGING 0

#include "native_client/src/shared/utils/debugging.h"

/* Returns true if the data extracted by the operand may be needed
 * for register sfi sandboxing.
 */
static Bool NaClNcvalKeepOperand(NaClOp *operand) {
  /* Operands in the ncval_seg_sfi (x86-64) validator are not inspected
   * by the validator, unless they are general purpose register, segment
   * registers, or memory references. The following list are all operand
   * specifiers that can't contain any of the above, and hence, are not
   * used by the validator. This list is used to safely determine if
   * an operand can be removed.
   */
  static NaClOpKind unnecessary_opkinds[] = {
    RegDR0,           /* Debug registers. */
    RegDR1,
    RegDR2,
    RegDR3,
    RegDR4,
    RegDR5,
    RegDR6,
    RegDR7,
    RegDR8,
    RegDR9,
    RegDR10,
    RegDR11,
    RegDR12,
    RegDR13,
    RegDR14,
    RegDR15,
    RegEFLAGS,        /* Program status and control registers. */
    RegRFLAGS,
    St_Operand,       /* Floating point stack registers. */
    RegST0,
    RegST1,
    RegST2,
    RegST3,
    RegST4,
    RegST5,
    RegST6,
    RegST7,
    Mmx_G_Operand,    /* Mxx registers. */
    Mmx_Gd_Operand,
    RegMMX0,
    RegMMX1,
    RegMMX2,
    RegMMX3,
    RegMMX4,
    RegMMX5,
    RegMMX6,
    RegMMX7,
    Xmm_G_Operand,    /* Xmm registers. */
    Xmm_Go_Operand,
    RegXMM0,
    RegXMM1,
    RegXMM2,
    RegXMM3,
    RegXMM4,
    RegXMM5,
    RegXMM6,
    RegXMM7,
    RegXMM8,
    RegXMM9,
    RegXMM10,
    RegXMM11,
    RegXMM12,
    RegXMM13,
    RegXMM14,
    RegXMM15,
  };
  size_t i;
  for (i = 0; i < NACL_ARRAY_SIZE(unnecessary_opkinds); ++i) {
    if (operand->kind == unnecessary_opkinds[i]) return FALSE;
  }
  return TRUE;
}

/* Returns the instruction mnemonic to use for an instruction. The
 * mnemonic is changed only if the (x86-64) ncval_reg_sfi validator
 * doesn't need to know the corresponding instruction mnemonic.
 */
static NaClMnemonic NaClNcvalSimplifyMnemonic(NaClModeledInst *inst) {
  /* List of white listed names that must be kept for the validator,
   * since they are used as part of the pattern checking of the validator.
   */
  static NaClMnemonic validator_names[] = {
    InstAnd,
    InstAdd,
    InstCall,
    InstInvalid,
    InstLea,
    InstMov,
    InstOr,
    InstPop,
    InstPush,
    InstSub
  };
  NaClMnemonic name = inst->name;
  size_t i;
  for (i = 0; i < NACL_ARRAY_SIZE(validator_names); ++i) {
    if (name == validator_names[i]) return name;
  }
  /* If not white listed, change to dont care. */
  if (NaClHasBit(inst->flags, NACL_IFLAG(ConditionalJump))) {
    return InstDontCareCondJump;
  }
  if (NaClHasBit(inst->flags, NACL_IFLAG(JumpInstruction))) {
    return InstDontCareJump;
  }
  return InstDontCare;
}

/* Fixes format strings used to define the instruction operand
 * to its simplified form, by removing any implicit braces used
 * when defining the operand for the instruction.
 */
static const char* RemoveImplicitFromFormat(const char* str) {
  size_t str_len = strlen(str);
  if ((str_len > 0) && (str[0] == '{') && (str[str_len-1] == '}')) {
    char* simp_str = (char*) malloc(str_len-1);
    if (NULL == simp_str) return str;  /* This should not happen. */
    strncpy(simp_str, str+1, str_len-1);
    simp_str[str_len-2] = '\0';
    return simp_str;
  }
  return str;
}

/* Simplify the given instruction and return the simplified instruction,
 * based on expectations of the (x86-64) ncval_reg_sfi validator.
 */
static void NaClNcvalSimplifyInst(NaClInstTables *inst_tables,
                                  NaClModeledInst *inst) {
  size_t i;
  NaClMnemonic simplified_name;
  size_t fill = 0;
  Bool is_partial = FALSE;

  if (NULL == inst) return;

  /* First simplify additional rules associated with the given instruction. */
  NaClNcvalSimplifyInst(inst_tables, inst->next_rule);

  /* Ignore invalid instructions. */
  if (NACLi_INVALID == inst->insttype) return;

  /* Remove unnecessary operands. */
  if (NaClHasBit(inst->flags, NACL_IFLAG(NaClIllegal))) {
    /* Remove all arguments. We don't need to process arguments.
     * Validation will fail regardless.
     */
    inst->num_operands = 0;
    inst->name = InstDontCare;
    is_partial = TRUE;
  } else {
    /* Simplify the instruction mnemonic. */
    simplified_name = NaClNcvalSimplifyMnemonic(inst);
    if (simplified_name != inst->name) {
      uint8_t num_operands;
      inst->name = simplified_name;
      is_partial = TRUE;

      /* We have simplified the modeled instruction, and hence
       * the modeled instruction is now a pattern, rather than'
       * an x86 instruction. Remove operands that aren't needed,
       * because the operand is never going to be used by the validator.
       */
      num_operands = inst->num_operands;
      for (i = 0; i < num_operands; i++) {
        if (NaClNcvalKeepOperand(&inst->operands[i])) {
          inst->operands[fill++] = inst->operands[i];
        } else {
          inst->num_operands--;
          is_partial = TRUE;
        }
      }
    }
  }

  /* Check if we modified anything. If so, we need to clean
   * up the instruction so that (all) useful information is printed
   * by the decoder print functions (i.e. as a pattern rather than
   * an x86 instruction).
   */
  if (is_partial) {
    uint8_t num_operands = inst->num_operands;
    NaClAddBits(inst->flags, NACL_IFLAG(PartialInstruction));
    for (i = 0; i < num_operands; i++) {
      if (NaClHasBit(inst->operands[i].flags, NACL_OPFLAG(OpImplicit))) {
        NaClRemoveBits(inst->operands[i].flags, NACL_OPFLAG(OpImplicit));
        inst->operands[i].format_string =
            RemoveImplicitFromFormat(inst->operands[i].format_string);
      }
    }
  }
}

/* Simplify instructions in the given trie node. */
static void NaClNcvalSimplifyNode(NaClInstTables *inst_tables,
                                  NaClModeledInstNode* node) {
  if (NULL == node) return;
  NaClNcvalSimplifyInst(inst_tables, node->matching_inst);
  NaClNcvalSimplifyNode(inst_tables, node->success);
  NaClNcvalSimplifyNode(inst_tables, node->fail);
}

void NaClNcvalInstSimplify(NaClInstTables* inst_tables) {
  int i;
  NaClInstPrefix prefix;

  /* Simplify the undefined instruction. */
  NaClNcvalSimplifyInst(inst_tables, inst_tables->undefined_inst);

  /* Simplify all instructions in the instruction opcode table. */
  for (prefix = NoPrefix; prefix < NaClInstPrefixEnumSize; ++prefix) {
    for (i = 0; i < NCDTABLESIZE; ++i) {
      NaClNcvalSimplifyInst(inst_tables, inst_tables->inst_table[i][prefix]);
    }
  }

  /* Simplify instructions in the instruction trie. */
  NaClNcvalSimplifyNode(inst_tables, inst_tables->inst_node_root);
}

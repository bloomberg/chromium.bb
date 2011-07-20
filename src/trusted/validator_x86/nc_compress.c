/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * Compresses the tables for the table generator.
 */

#ifndef NACL_TRUSTED_BUT_NOT_TCB
#error("This file is not meant for use in the TCB")
#endif

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>

#include "native_client/src/trusted/validator_x86/nc_compress.h"

#include "native_client/src/include/portability.h"
#include "native_client/src/include/portability_io.h"
#include "native_client/src/shared/utils/flags.h"
#include "native_client/src/shared/platform/nacl_log.h"
#include "native_client/src/trusted/validator/x86/x86_insts.h"
#include "native_client/src/trusted/validator_x86/nacl_regsgen.h"
#include "native_client/src/trusted/validator_x86/ncdecode_forms.h"
#include "native_client/src/trusted/validator_x86/ncdecode_st.h"

/* To turn on debugging of instruction decoding, change value of
 * DEBUGGING to 1.
 */
#define DEBUGGING 0

#include "native_client/src/shared/utils/debugging.h"

/* Returns true if the given instructions are (runtime) equal. */
static Bool NaClInstSame(const NaClModeledInst* inst1,
                         const NaClModeledInst* inst2) {
  if (inst1 == inst2) return TRUE;
  if ((inst1 == NULL) || (inst2 == NULL)) return FALSE;
  if (inst1->insttype != inst2->insttype) return FALSE;
  if (inst1->flags != inst2->flags) return FALSE;
  if (inst1->name != inst2->name) return FALSE;
  if (inst1->opcode_ext != inst2->opcode_ext) return FALSE;
  if (inst1->num_operands != inst2->num_operands) return FALSE;
  if (inst1->operands != inst2->operands) return FALSE;
  if (!NaClInstSame(inst1->next_rule, inst2->next_rule)) return FALSE;
  return TRUE;
}

/* Returns true if the given operands are equal. */
static Bool NaClOpSame(const NaClOp* op1, const NaClOp* op2) {
  if ((op1->kind == op2->kind) && (op1->flags == op2->flags)) {
    if (NULL == op1->format_string) {
      return NULL == op2->format_string;
    } else if (NULL != op2->format_string) {
      return 0 == strcmp(op1->format_string, op2->format_string);
    }
  }
  return FALSE;
}

/* Compress the operands in the given instruction. */
static void NaClInstOpCompress(NaClInstTables* inst_tables,
                               NaClModeledInst* inst) {
  size_t i;
  Bool found = FALSE;  /* until proven otherwise */
  if (NULL == inst) return;
  if (inst->num_operands <= *inst_tables->ops_compressed_size) {
    /* Note: Be sure to not overflow compressed array by stopping when
     * there is no more room for the operands of the given instruction.
     */
    for (i = 0;
         i <= *inst_tables->ops_compressed_size - inst->num_operands;
         ++i) {
      size_t j;
      Bool matches = TRUE;  /* until proven otherwise */
      for (j = 0; j < inst->num_operands; ++j) {
        if (!NaClOpSame(inst->operands + j,
                        inst_tables->ops_compressed + i + j)) {
          matches = FALSE;
          break;
        }
      }
      if (matches) {
        /* Operands already in compressed table, use copy. */
        inst->operands = inst_tables->ops_compressed + i;
        found = TRUE;
        break;
      }
    }
  }
  if (!found) {
    /* If reached, not in compressed table. Add. */
    size_t new_size =
        *inst_tables->ops_compressed_size + inst->num_operands;
    if (new_size > NACL_MAX_OPERANDS_TOTAL) {
      NaClFatal("Not enough operand space to compress");
    }
    for (i = 0; i < inst->num_operands; ++i) {
      size_t index = *inst_tables->ops_compressed_size + i;
      inst_tables->ops_compressed[index].kind = inst->operands[i].kind;
      inst_tables->ops_compressed[index].flags = inst->operands[i].flags;
      inst_tables->ops_compressed[index].format_string =
          inst->operands[i].format_string;
    }
    inst->operands = inst_tables->ops_compressed +
        *inst_tables->ops_compressed_size;
    *inst_tables->ops_compressed_size = new_size;
  }
}

/* Records the number of instructions that compression was attempted on. */
static size_t num_uncompressed_instructions = 0;

/* Compress the given instruction. */
static NaClModeledInst* NaClInstCompress(NaClInstTables* inst_tables,
                                           NaClModeledInst* inst) {
  size_t i;
  if (inst == NULL) return NULL;
  ++num_uncompressed_instructions;

  /* First compress additional rules associated with the given
   * instruction, so that when we compare this instruction, the
   * compression of the additional rules will automatically be counted.
   */
  inst->next_rule = NaClInstCompress(inst_tables, inst->next_rule);

  /* Compress the given instruction. */
  NaClInstOpCompress(inst_tables, inst);

  /* Now see if we already have such an instruction. If so, use it. */
  for (i = 0; i < *inst_tables->inst_compressed_size; ++i) {
    NaClModeledInst* comp_inst = (*inst_tables->inst_compressed)[i];
    if (NaClInstSame(inst, comp_inst)) return comp_inst;
  }

  /* If reached, no such instruction exists, put it into the table. */
  (*inst_tables->inst_compressed)[i] = inst;
  ++(*inst_tables->inst_compressed_size);
  return inst;
}

/* Compress operands of all instructions reachable from
 * the given node.
 */
static void NaClOpNodeCompress(NaClInstTables* inst_tables,
                               NaClModeledInstNode* node) {
  if (NULL == node) return;
  node->matching_inst = NaClInstCompress(inst_tables, node->matching_inst);
  NaClOpNodeCompress(inst_tables, node->success);
  NaClOpNodeCompress(inst_tables, node->fail);
}

/* Walks over the modeled instructions and replaces duplicate
 * operand entries.
 */
void NaClOpCompress(NaClInstTables* inst_tables) {
  int i;
  NaClInstPrefix prefix;
  fprintf(stderr, "Note: %"NACL_PRIuS" operands before compression, ",
          *inst_tables->operands_size);
  /* Compress the undefined instruction. Note: Must appear first,
   * so that we know the location of undefined for static initialization
   * in ncdis_decode_tables.c
   */
  *inst_tables->undefined_inst =
      NaClInstCompress(inst_tables, *inst_tables->undefined_inst);

  /* Compress all operands of instructions in the instruction opcode tables. */
  for (prefix = NoPrefix; prefix < NaClInstPrefixEnumSize; ++prefix) {
    for (i = 0; i < NCDTABLESIZE; ++i) {
      (*inst_tables->inst_table)[i][prefix]=
          NaClInstCompress(inst_tables, (*inst_tables->inst_table)[i][prefix]);
    }
  }

  /* Compress all operands of instructions in the instruction trie. */
  NaClOpNodeCompress(inst_tables, *inst_tables->inst_node_root);
  fprintf(stderr, "%"NACL_PRIuS" after.\n",
          *inst_tables->ops_compressed_size);
  fprintf(stderr, "Note: %"NACL_PRIuS" instruction before compression, "
          "%"NACL_PRIuS" after.\n",
          num_uncompressed_instructions,
          *inst_tables->inst_compressed_size);
}

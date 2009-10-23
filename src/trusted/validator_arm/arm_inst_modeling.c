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

/*
 * Models and generates the ARM instruction set.
 */
#include <string.h>

#include "native_client/src/trusted/validator_arm/arm_inst_modeling.h"
#include "native_client/src/trusted/validator_arm/arm_branches.h"
#include "native_client/src/trusted/validator_arm/arm_coprocessor.h"
#include "native_client/src/trusted/validator_arm/arm_data_ops.h"
#include "native_client/src/trusted/validator_arm/arm_exceptions.h"
#include "native_client/src/trusted/validator_arm/arm_extended_ops.h"
#include "native_client/src/trusted/validator_arm/arm_insts.h"
#include "native_client/src/trusted/validator_arm/arm_load_stores.h"
#include "native_client/src/trusted/validator_arm/arm_misc_ops.h"
#include "native_client/src/trusted/validator_arm/arm_mulops.h"
#include "native_client/src/trusted/validator_arm/arm_parallel.h"
#include "native_client/src/trusted/validator_arm/arm_swap_insts.h"
#include "native_client/src/shared/utils/flags.h"
#include "native_client/src/shared/utils/formatting.h"

#define DEBUGGING 0
#include "native_client/src/trusted/validator_arm/debugging.h"

/*
 * ********************************************************
 * This section defines flags for controlling what instructions
 * get modeled.
 * ********************************************************
 */

/* True if (only) branch instructions should be added to instruction set. */
static Bool FLAGS_add_branches = FALSE;

/* True if (only) (common) data operations should be added to the
 * instruction set. */
static Bool FLAGS_add_data_ops = FALSE;

/* True if (only) multiplication operations should be added to the
 * instuction set.
 */
static Bool FLAGS_add_mult_ops = FALSE;

/* True if (only) parallel operations should be added to the instruction set. */
static Bool FLAGS_add_parallel_ops = FALSE;

/* True if (only) extended (ARM) operations should be added to the
 * instruction set.
 */
static Bool FLAGS_add_extend_ops = FALSE;

/* True if (only) miscellaneous (ARM) operations should be added to the
 * instruction set.
 */
static Bool FLAGS_add_misc_ops = FALSE;

/* True if (only) load/store operatations should be added to the
 * instruction set.
 */
static Bool FLAGS_add_ls_ops = FALSE;

/* True if (only) (ARM) swap operations should be added to the
 * instruction set.
 */
static Bool FLAGS_add_swap_ops = FALSE;

/* True if (only) exception handling operations should be added to the
 * instruction set.
 */
static Bool FLAGS_add_except_ops = FALSE;

/* True if (only) coprocessor instructions should be added to the
 * instruction set.
 */
static Bool FLAGS_add_cp_ops = FALSE;

/* Note: By default all ARM instructions are built. If any of the flags
 * below are TRUE, then only the instructions defined by the true flags
 * will be added to the instruction set.
 */
int GrokArmBuildFlags(int argc, const char *argv[]) {
  int i;
  int new_argc = (argc == 0 ? 0 : 1);
  for (i = 1; i < argc; ++i) {
    if (GrokBoolFlag("--add_branches", argv[i],
                     &FLAGS_add_branches) ||
        GrokBoolFlag("--add_data_ops", argv[i],
                     &FLAGS_add_data_ops) ||
        GrokBoolFlag("--add_mult_ops", argv[i],
                     &FLAGS_add_mult_ops) ||
        GrokBoolFlag("--add_parallel_ops", argv[i],
                     &FLAGS_add_parallel_ops) ||
        GrokBoolFlag("--add_extend_ops", argv[i],
                     &FLAGS_add_extend_ops) ||
        GrokBoolFlag("--add_misc_ops", argv[i],
                     &FLAGS_add_misc_ops) ||
        GrokBoolFlag("--add_ls_ops", argv[i],
                     &FLAGS_add_ls_ops) ||
        GrokBoolFlag("--add_swap_ops", argv[i],
                     &FLAGS_add_swap_ops) ||
        GrokBoolFlag("--add_except_ops", argv[i],
                     &FLAGS_add_except_ops) ||
        GrokBoolFlag("--add_cp_ops", argv[i],
                     &FLAGS_add_cp_ops)) {
    } else {
      /* Default to not a flag. */
      argv[new_argc++] = argv[i];
    }
  }
  return new_argc;
}


/*
 * ********************************************************
 * This section models the construction of modeled ARM minstructions.
 * ********************************************************
 */

/*
 * Assign lhs to a copy of the rhs values.
 */
static void AssignOpInfo(ModeledOpInfo* lhs, const ModeledOpInfo* rhs) {
  memcpy(lhs, rhs, sizeof(OpInfo));
}

ModeledOpInfo* CopyOfOpInfo(const ModeledOpInfo* op) {
  ModeledOpInfo* copy = (ModeledOpInfo*) malloc(sizeof(ModeledOpInfo));
  AssignOpInfo(copy, op);
  return copy;
}

/* NOTE: zero initialized */
ModeledArmInstructionSet modeled_arm_instruction_set;


void AddInstruction(const ModeledOpInfo* instruction) {
  modeled_arm_instruction_set.instructions[modeled_arm_instruction_set.size] =
      instruction;
  DEBUG(PrintIndexedInstruction(arm_instruction_set.size));
  if (modeled_arm_instruction_set.size + 2 >= MAX_ARM_INSTRUCTION_SET_SIZE) {
    printf("Too many instructions specified in instruction set! "\
           "Ignoring request.\n");
  } else {
    modeled_arm_instruction_set.size++;
  }
}

void BuildArmInstructionSet() {
  Bool add_all_ops;

  /* Before starting, be sure that instruction set isn't defined. */
  if (0 < modeled_arm_instruction_set.size) return;

  add_all_ops =
      !(FLAGS_add_branches || FLAGS_add_data_ops || FLAGS_add_mult_ops ||
        FLAGS_add_parallel_ops || FLAGS_add_extend_ops || FLAGS_add_misc_ops ||
        FLAGS_add_ls_ops || FLAGS_add_swap_ops || FLAGS_add_except_ops ||
        FLAGS_add_cp_ops);

  /* Add branch instructions to the instruction set. */
  if (add_all_ops || FLAGS_add_branches) {
    BuildArmBranchInstructions();
  }

  /* Add data operations to the instruction set. */
  if (add_all_ops || FLAGS_add_data_ops) {
    BuildArmDataInstructions();
  }

  /* Add multiply instructions. */
  if (add_all_ops || FLAGS_add_mult_ops) {
    BuildArmMulopInstructions();
  }

  /* Add parallel add/subtract instructions */
  if (add_all_ops || FLAGS_add_parallel_ops) {
    BuildParallelOps();
  }

  /* Add extend instructions */
  if (add_all_ops || FLAGS_add_extend_ops) {
    BuildExtendedOps();
  }

  /* Add miscellaneous ops. */
  if (add_all_ops || FLAGS_add_misc_ops) {
    BuildMiscellaneousOps();
  }

  /* Add load and stores. */
  if (add_all_ops || FLAGS_add_ls_ops) {
    BuildLoadStores();
  }

  /* Add swap instructions */
  if (add_all_ops || FLAGS_add_swap_ops) {
    BuildSwapInstructions();
  }

  /* Add exception-generating instructions */
  if (add_all_ops || FLAGS_add_except_ops) {
    BuildExceptionGenerating();
  }

  /* Add coprocessor instructions*/
  if (add_all_ops || FLAGS_add_cp_ops) {
    BuildCoprocessorOps();
  }
}

/*
 * ********************************************************
 * This section defines functions to extract values from
 * the modeled instruction, and print it.
 * ********************************************************
 */

/*
 * Prints out the specified field of an OpInfo structure into
 * the buffer at the cursor position.
 *
 * %n - name field.
 * %k - inst_kind field.
 * %a - inst_access field.
 * %t - inst_type field.
 * %s - arm_safe field.
 * %f - describe_format field.
 * %b - num_bytes field.
 * %e - expected_values field.
 */
static Bool ModeledOpInfoDirective(char directive,
                                   char* buffer,
                                   size_t buffer_size,
                                   void* data,
                                   size_t* cursor) {
  ModeledOpInfo* op = (ModeledOpInfo*) data;
  switch (directive) {
    case 'n':
      FormatAppend(buffer, buffer_size, op->name, cursor);
      return TRUE;
    case 'k':
      FormatAppend(buffer,
                   buffer_size,
                   GetArmInstKindName(op->inst_kind),
                   cursor);
      return TRUE;
    case 'a':
      FormatAppend(buffer, buffer_size, op->inst_access, cursor);
      return TRUE;
    case 't':
      FormatAppend(buffer,
                   buffer_size,
                   GetArmInstTypeName(op->inst_type),
                   cursor);
      return TRUE;
    case 's':
      FormatAppend(buffer, buffer_size, GetBoolName(op->arm_safe), cursor);
      return TRUE;
    case 'f':
      FormatAppend(buffer, buffer_size, op->describe_format, cursor);
      return TRUE;
    case 'b':
      ValueAppend(buffer, buffer_size, op->num_bytes, cursor);
      return TRUE;
    case 'e':
      InstValuesAppend(buffer,
                       buffer_size,
                       &(op->expected_values),
                       op->inst_type,
                       cursor);
      return TRUE;
    default:
      return FALSE;
  }
}

/* Append text describing the given OpInfo data into the buffer. */
static void ModeledOpInfoAppend(char* buffer,
                                size_t buffer_size,
                                const ModeledOpInfo* op,
                                size_t* cursor) {
  FormatDataAppend(buffer,
                   buffer_size,
                   "%s\t%k\t%t \t%n:\t%f\n\t%a\n\t%e",
                   (void*) op,
                   ModeledOpInfoDirective,
                   cursor);
}

/*
 * Generate descriptions of the given indexed instruction.
 *
 * %v - Insert the instruction values of the corresponding indexed instruction.
 * %o - Insert the OpInfo values of the corresponding indexed instruction.
 * %i - Insert the index of the indexed instruction.
 */
static Bool IndexedInstructionDirective(char directive,
                                        char* buffer,
                                        size_t buffer_size,
                                        void* data,
                                        size_t* cursor) {
  int index = *((int*) data);
  switch (directive) {
    case 'v': {
        const ModeledOpInfo* op =
            modeled_arm_instruction_set.instructions[index];
        InstValuesAppend(buffer,
                         buffer_size,
                         &(op->expected_values),
                         op->inst_type,
                         cursor);
        return TRUE;
      }
    case 'o': {
        const ModeledOpInfo* op =
            modeled_arm_instruction_set.instructions[index];
        ModeledOpInfoAppend(buffer, buffer_size, op, cursor);
        return TRUE;
      }
    case 'i':
      ValueAppend(buffer, buffer_size, index, cursor);
      return TRUE;
    default:
      return FALSE;
  }
}

/*
 * Prints out the instruction defined by the given OpInfo structure,
 * assuming it has the given index, to the given buffer. When the
 * buffer fills, no additional text is added to the buffer, even
 * though the cursor is incremented accordingly.
 */
static void IndexedInstructionAppend(char* buffer,
                                     size_t buffer_size,
                                     int index,
                                     size_t* cursor) {
  FormatDataAppend(buffer,
                   buffer_size,
                   "[%i]: %o",
                   (void*) (&index),
                   IndexedInstructionDirective,
                   cursor);
}

static Bool DescribeIndexedInstruction(char* buffer,
                                       size_t buffer_size,
                                       int index) {
  size_t cursor = 0;
  IndexedInstructionAppend(buffer, buffer_size, index, &cursor);
  return cursor < buffer_size;
}

Bool PrintIndexedInstruction(int index) {
  char buffer[INST_BUFFER_SIZE];
  Bool results = DescribeIndexedInstruction(buffer, sizeof(buffer), index);
  printf("%s\n", buffer);
  return results;
}

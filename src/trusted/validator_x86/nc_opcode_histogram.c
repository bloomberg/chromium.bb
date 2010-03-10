/*
 * Copyright 2008 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

/*
 * nc_opcode_histogram.c - Collects histogram information while validating.
 */

#include "native_client/src/trusted/validator_x86/nc_opcode_histogram.h"

#include "native_client/src/shared/platform/nacl_log.h"
#include "native_client/src/trusted/validator_x86/nc_inst_state.h"
#include "native_client/src/trusted/validator_x86/ncvalidate_iter.h"

/* Holds a histogram of the (first) byte of the found opcodes for each
 * instruction.
 */
typedef struct NaClOpcodeHistogram {
  uint32_t opcode_histogram[256];
} NaClOpcodeHistogram;

NaClOpcodeHistogram* NaClOpcodeHistogramMemoryCreate(
    NaClValidatorState* state) {
  int i;
  NaClOpcodeHistogram* histogram =
      (NaClOpcodeHistogram*) malloc(sizeof(NaClOpcodeHistogram));
  if (histogram == NULL) {
    NaClValidatorMessage(LOG_FATAL, state,
                         "Out of memory, can't build histogram\n");
  }
  for (i = 0; i < 256; ++i) {
    histogram->opcode_histogram[i] = 0;
  }
  return histogram;
}

void NaClOpcodeHistogramMemoryDestroy(NaClValidatorState* state,
                                      NaClOpcodeHistogram* histogram) {
  free(histogram);
}

void NaClOpcodeHistogramRecord(NaClValidatorState* state,
                               NaClInstIter* iter,
                               NaClOpcodeHistogram* histogram) {
  NaClInstState* inst_state = NaClInstIterGetState(iter);
  NaClInst* inst = NaClInstStateInst(inst_state);
  if (inst->name != InstUndefined) {
    histogram->opcode_histogram[inst->opcode[inst->num_opcode_bytes - 1]]++;
  }
}

void NaClOpcodeHistogramPrintStats(FILE* f,
                                   NaClValidatorState* state,
                                   NaClOpcodeHistogram* histogram) {
  int i;
  int printed_in_this_row = 0;
  fprintf(f, "\nOpcode Histogram:\n");
  for (i = 0; i < 256; ++i) {
    if (0 != histogram->opcode_histogram[i]) {
      fprintf(f, "%"NACL_PRId32"\t0x%02x\t", histogram->opcode_histogram[i], i);
      ++printed_in_this_row;
      if (printed_in_this_row > 3) {
        printed_in_this_row = 0;
        fprintf(f, "\n");
      }
    }
  }
  if (0 != printed_in_this_row) {
    fprintf(f, "\n");
  }
}

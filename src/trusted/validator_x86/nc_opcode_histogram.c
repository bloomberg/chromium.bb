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
typedef struct OpcodeHistogram {
  uint32_t opcode_histogram[256];
} OpcodeHistogram;

OpcodeHistogram* NcOpcodeHistogramMemoryCreate(NcValidatorState* state) {
  int i;
  OpcodeHistogram* histogram =
      (OpcodeHistogram*) malloc(sizeof(OpcodeHistogram));
  if (histogram == NULL) {
    NcValidatorMessage(LOG_FATAL, state,
                       "Out of memory, can't build histogram\n");
  }
  for (i = 0; i < 256; ++i) {
    histogram->opcode_histogram[i] = 0;
  }
  return histogram;
}

void NcOpcodeHistogramMemoryDestroy(NcValidatorState* state,
                                    OpcodeHistogram* histogram) {
  free(histogram);
}

void NcOpcodeHistogramRecord(NcValidatorState* state,
                             NcInstIter* iter,
                             OpcodeHistogram* histogram) {
  NcInstState* inst_state = NcInstIterGetState(iter);
  Opcode* opcode = NcInstStateOpcode(inst_state);
  if (opcode->name != InstUndefined) {
    histogram->opcode_histogram[opcode->opcode[opcode->num_opcode_bytes - 1]]++;
  }
}

void NcOpcodeHistogramPrintStats(FILE* f,
                                 NcValidatorState* state,
                                 OpcodeHistogram* histogram) {
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

/*
 * Copyright 2008, Google Inc.
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
      fprintf(f, "%"PRId32"\t0x%02x\t", histogram->opcode_histogram[i], i);
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

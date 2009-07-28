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
 * Defines an instruction (decoder) iterator that processes code segments.
 */

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include "native_client/src/trusted/validator_x86/nc_inst_iter.h"
#include "native_client/src/trusted/validator_x86/nc_inst_trans.h"
#include "native_client/src/trusted/validator_x86/nc_inst_state.h"
#include "native_client/src/trusted/validator_x86/nc_inst_state_internal.h"
#include "native_client/src/trusted/validator_x86/nc_segment.h"
#include "native_client/src/trusted/validator_x86/ncop_exps.h"

/* To turn on debugging of instruction decoding, change value of
 * DEBUGGING to 1.
 */
#define DEBUGGING 0

#include "native_client/src/shared/utils/debugging.h"

NcInstIter* NcInstIterCreateWithLookback(
    NcSegment* segment,
    size_t lookback_size) {
  size_t i;
  NcInstIter* iter;
  /* Guarantee we don't wrap around while computing buffer index updates. */
  assert(((lookback_size + 1) * 2 + 1) > lookback_size);
  iter = (NcInstIter*) malloc(sizeof(NcInstIter));
  iter->segment = segment;
  iter->index = 0;
  iter->buffer_size = lookback_size + 1;
  iter->buffer_index = 0;
  iter->buffer = (NcInstState*) calloc(iter->buffer_size, sizeof(NcInstState));
  for (i = 0; i < iter->buffer_size; ++i) {
    iter->buffer[i].opcode = NULL;
  }
  return iter;
}

NcInstIter* NcInstIterCreate(NcSegment* segment) {
  return NcInstIterCreateWithLookback(segment, 0);
}

void NcInstIterDestroy(NcInstIter* iter) {
  free(iter->buffer);
  free(iter);
}

NcInstState* NcInstIterGetState(NcInstIter* iter) {
  NcInstState* state = &iter->buffer[iter->buffer_index];
  if (NULL == state->opcode) {
    DecodeInstruction(iter, state);
  }
  return state;
}

NcInstState* NcInstIterGetLookbackState(NcInstIter* iter, size_t distance) {
  NcInstState* state;
  assert(distance < iter->buffer_size);
  state = &iter->buffer[((iter->buffer_index + iter->buffer_size) - distance)
                        % iter->buffer_size];
  if (NULL == state->opcode) {
    DecodeInstruction(iter, state);
  }
  return state;
}

Bool NcInstIterHasNext(NcInstIter* iter) {
  DEBUG(printf("iter has next index %"PRIxMemorySize" < %"PRIxMemorySize"\n",
               iter->index, iter->segment->size));
  return iter->index < iter->segment->size;
}

void NcInstIterAdvance(NcInstIter* iter) {
  NcInstState* state;
  if (iter->index >= iter->segment->size) {
    fprintf(stderr, "*ERROR* NcInstIterAdvance with no next element.\n");
    exit(1);
  }
  state = NcInstIterGetState(iter);
  iter->index += state->length;
  iter->buffer_index = (iter->buffer_index + 1) % iter->buffer_size;
  DEBUG(
      printf(
          "iter advance: index %"PRIxMemorySize", buffer index %"PRIuS"\n",
          iter->index, iter->buffer_index));
  iter->buffer[iter->buffer_index].opcode = NULL;
}

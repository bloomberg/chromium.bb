/*
 * Copyright 2009 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
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
  iter->inst_count = 0;
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

Bool NcInstIterHasLookbackState(NcInstIter* iter, size_t distance) {
  return distance < iter->buffer_size && distance <= iter->inst_count;
}

NcInstState* NcInstIterGetLookbackState(NcInstIter* iter, size_t distance) {
  NcInstState* state;
  assert(distance < iter->buffer_size);
  assert(distance <= iter->inst_count);
  state = &iter->buffer[((iter->buffer_index + iter->buffer_size) - distance)
                        % iter->buffer_size];
  if (NULL == state->opcode) {
    DecodeInstruction(iter, state);
  }
  return state;
}

Bool NcInstIterHasNext(NcInstIter* iter) {
  DEBUG(printf("iter has next index %"NACL_PRIxMemorySize
               " < %"NACL_PRIxMemorySize"\n",
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
  ++iter->inst_count;
  iter->buffer_index = (iter->buffer_index + 1) % iter->buffer_size;
  DEBUG(
      printf(
          "iter advance: index %"NACL_PRIxMemorySize", "
          "buffer index %"NACL_PRIuS"\n",
          iter->index, iter->buffer_index));
  iter->buffer[iter->buffer_index].opcode = NULL;
}

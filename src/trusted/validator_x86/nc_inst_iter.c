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

NaClInstIter* NaClInstIterCreateWithLookback(
    NaClSegment* segment,
    size_t lookback_size) {
  size_t i;
  NaClInstIter* iter;
  /* Guarantee we don't wrap around while computing buffer index updates. */
  assert(((lookback_size + 1) * 2 + 1) > lookback_size);
  iter = (NaClInstIter*) malloc(sizeof(NaClInstIter));
  iter->segment = segment;
  iter->index = 0;
  iter->inst_count = 0;
  iter->buffer_size = lookback_size + 1;
  iter->buffer_index = 0;
  iter->buffer = (NaClInstState*)
      calloc(iter->buffer_size, sizeof(NaClInstState));
  for (i = 0; i < iter->buffer_size; ++i) {
    iter->buffer[i].inst = NULL;
  }
  return iter;
}

NaClInstIter* NaClInstIterCreate(NaClSegment* segment) {
  return NaClInstIterCreateWithLookback(segment, 0);
}

void NaClInstIterDestroy(NaClInstIter* iter) {
  free(iter->buffer);
  free(iter);
}

NaClInstState* NaClInstIterGetState(NaClInstIter* iter) {
  NaClInstState* state = &iter->buffer[iter->buffer_index];
  if (NULL == state->inst) {
    NaClDecodeInst(iter, state);
  }
  return state;
}

Bool NaClInstIterHasLookbackState(NaClInstIter* iter, size_t distance) {
  return distance < iter->buffer_size && distance <= iter->inst_count;
}

NaClInstState* NaClInstIterGetLookbackState(NaClInstIter* iter,
                                            size_t distance) {
  NaClInstState* state;
  assert(distance < iter->buffer_size);
  assert(distance <= iter->inst_count);
  state = &iter->buffer[((iter->buffer_index + iter->buffer_size) - distance)
                        % iter->buffer_size];
  if (NULL == state->inst) {
    NaClDecodeInst(iter, state);
  }
  return state;
}

Bool NaClInstIterHasNext(NaClInstIter* iter) {
  DEBUG(printf("iter has next index %"NACL_PRIxNaClMemorySize
               " < %"NACL_PRIxNaClMemorySize"\n",
               iter->index, iter->segment->size));
  return iter->index < iter->segment->size;
}

void NaClInstIterAdvance(NaClInstIter* iter) {
  NaClInstState* state;
  if (iter->index >= iter->segment->size) {
    fprintf(stderr, "*ERROR* NaClInstIterAdvance with no next element.\n");
    exit(1);
  }
  state = NaClInstIterGetState(iter);
  iter->index += state->length;
  ++iter->inst_count;
  iter->buffer_index = (iter->buffer_index + 1) % iter->buffer_size;
  DEBUG(
      printf(
          "iter advance: index %"NACL_PRIxNaClMemorySize", "
          "buffer index %"NACL_PRIuS"\n",
          iter->index, iter->buffer_index));
  iter->buffer[iter->buffer_index].inst = NULL;
}

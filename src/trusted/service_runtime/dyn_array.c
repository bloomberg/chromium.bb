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
 * Implementation of dynamic arrays.
 */

#include "native_client/src/include/portability.h"

#define DYN_ARRAY_DEBUG 1

#include <stdlib.h>
#include <string.h>

#if DYN_ARRAY_DEBUG
# include "native_client/src/shared/platform/nacl_log.h"
#endif

#include "native_client/src/trusted/service_runtime/dyn_array.h"


static INLINE int BitsToAllocWords(int nbits) {
  return (nbits + kBitsPerWord - 1) >> kWordIndexShift;
}


static INLINE int BitsToIndex(int nbits) {
  return nbits >> kWordIndexShift;
}


static INLINE int BitsToOffset(int nbits) {
  return nbits & (kBitsPerWord - 1);
}


int DynArrayCtor(struct DynArray  *dap,
                 int              initial_size) {
  if (initial_size <= 0) {
    initial_size = 32;
  }
  dap->num_entries = 0u;
  dap->ptr_array = calloc(initial_size, sizeof *dap->ptr_array);
  if (NULL == dap->ptr_array) {
    return 0;
  }
  dap->available = calloc(BitsToAllocWords(initial_size),
                          sizeof *dap->available);
  if (NULL == dap->available) {
    free(dap->ptr_array);
    dap->ptr_array = NULL;
    return 0;
  }
  dap->avail_ix = 0;  /* hint */

  dap->ptr_array_space = initial_size;
  return 1;
}


void DynArrayDtor(struct DynArray *dap) {
  dap->num_entries = 0;  /* assume user has freed entries */
  free(dap->ptr_array);
  dap->ptr_array = NULL;
  dap->ptr_array_space = 0;
  free(dap->available);
  dap->available = NULL;
}


void *DynArrayGet(struct DynArray *dap,
                  int             idx) {
  if ((unsigned) idx < (unsigned) dap->num_entries) {
    return dap->ptr_array[idx];
  }
  return NULL;
}


int DynArraySet(struct DynArray *dap,
                int             idx,
                void            *ptr) {
  int desired_space;
  int tmp;
  int ix;

  for (desired_space = dap->ptr_array_space;
       idx >= desired_space;
       desired_space = tmp) {
    tmp = 2 * desired_space;
    if (tmp < desired_space) {
      return 0;
    }
  }
  if (desired_space != dap->ptr_array_space) {
    /* need to grow */

    void      **new_space;
    uint32_t  *new_avail;
    size_t    new_avail_nwords;
    size_t    old_avail_nwords;

    new_space = realloc(dap->ptr_array, desired_space * sizeof *new_space);
    if (NULL == new_space) {
      return 0;
    }
    memset((void *) (new_space + dap->ptr_array_space),
           0,
           (desired_space - dap->ptr_array_space) * sizeof *new_space);
    dap->ptr_array = new_space;

    old_avail_nwords = BitsToAllocWords(dap->ptr_array_space);
    new_avail_nwords = BitsToAllocWords(desired_space);

    new_avail = realloc(dap->available,
                        new_avail_nwords * sizeof *new_avail);
    if (NULL == new_avail) {
      return 0;
    }
    memset((void *) &new_avail[old_avail_nwords],
           0,
           (new_avail_nwords - old_avail_nwords) * sizeof *new_avail);
    dap->available = new_avail;

    dap->ptr_array_space = desired_space;
  }
  dap->ptr_array[idx] = ptr;
  ix = BitsToIndex(idx);
#if DYN_ARRAY_DEBUG
  NaClLog(4, "Set(%d,%p) @ix %d: 0x%08x\n", idx, ptr, ix, dap->available[ix]);
#endif
  if (NULL != ptr) {
    dap->available[ix] |= (1 << BitsToOffset(idx));
  } else {
    dap->available[ix] &= ~(1 << BitsToOffset(idx));
    if (ix < dap->avail_ix) {
      dap->avail_ix = ix;
    }
  }
#if DYN_ARRAY_DEBUG
  NaClLog(4, "After @ix %d: 0x%08x, avail_ix %d\n",
          ix, dap->available[ix], dap->avail_ix);
#endif
  if (dap->num_entries <= idx) {
    dap->num_entries = idx + 1;
  }
  return 1;
}


int DynArrayFirstAvail(struct DynArray *dap) {
  int ix;
  int last_ix;
  int avail_pos;

  last_ix = BitsToAllocWords(dap->ptr_array_space);

#if DYN_ARRAY_DEBUG
  for (ix = 0; ix < last_ix; ++ix) {
    NaClLog(4, "ix %d: 0x%08x\n", ix, dap->available[ix]);
  }
#endif
  for (ix = dap->avail_ix; ix < last_ix; ++ix) {
    if (0U != ~dap->available[ix]) {
#if DYN_ARRAY_DEBUG
      NaClLog(4, "found first not-all-ones ix %d\n", ix);
#endif
      dap->avail_ix = ix;
      break;
    }
  }
  if (ix < last_ix) {
    avail_pos = ffs(~dap->available[ix]) - 1;
    avail_pos += ix << kWordIndexShift;
  } else {
    avail_pos = dap->ptr_array_space;
  }
  return avail_pos;
}

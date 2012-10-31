/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "native_client/src/shared/platform/aligned_malloc.h"

#include <stdlib.h>


void *NaClAlignedMalloc(size_t size, size_t alignment) {
  void *block;
  if (posix_memalign(&block, alignment, size) != 0)
    return NULL;
  return block;
}

void NaClAlignedFree(void *block) {
  free(block);
}

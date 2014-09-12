/*
 * Copyright (c) 2014 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * These macros parameterize the dlmalloc code the way we want it.
 * See dlmalloc/malloc.c for the details.
 */

#define LACKS_TIME_H            1
#define USE_LOCKS               1
#define USE_SPIN_LOCKS          1
#define HAVE_MMAP               0
#define HAVE_MREMAP             0

/*
 * We define the following to build just the core functions. Once I/O syscalls
 * are ready, we can remove these macros and build everything.
 */
#define NO_MALLINFO             1
#define NO_MALLOC_STATS         1

/* @IGNORE_LINES_FOR_CODE_HYGIENE[1] */
#include "native_client/src/third_party/dlmalloc/malloc.c"

/*
 * Crufty newlib internals use these entry points rather than the standard ones.
 */

void *_malloc_r(struct _reent *ignored, size_t size) {
  return malloc(size);
}

void *_calloc_r(struct _reent *ignored, size_t n, size_t size) {
  return calloc(n, size);
}

void *_realloc_r(struct _reent *ignored, void *ptr, size_t size) {
  return realloc(ptr, size);
}

void _free_r(struct _reent *ignored, void *ptr) {
  free(ptr);
}

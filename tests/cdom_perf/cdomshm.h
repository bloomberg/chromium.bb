/*
 * Copyright 2008 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */
/*
 * cdomshm.h - some helper stuff for sharing C-generated HTML with JavaScript
 *
 * These structs are used to manage a shared memory buffer as a big text
 * buffer that grows through append operations.
 */
#ifndef NATIVE_CLIENT_TESTS_CDOM_PERF_CDOMSHM_H_
#define NATIVE_CLIENT_TESTS_CDOM_PERF_CDOMSHM_H_

typedef struct big_buffer {
  char *buf;        /* pointer to the shared memory region */
  size_t index;     /* next byte to write */
  size_t max;       /* end of buffer/shared memory region */
  int failed;       /* failure (e.g. attempt to write past end) */
} BigBuffer;

/* Helper variable for NaCl shared memory link */
extern BigBuffer g_BigBuffer;

/* BBWrite() appends text to end of the BigBuffer. bb will commonly be
 * a pointer to g_BigBuffer.
 */
void BBWrite(const char *s, BigBuffer *bb);

#endif

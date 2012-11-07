/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* we need to make sure that _start is the very first function */
/* NOTE: gcc reorders functions in a file arbitrarily */

/* @IGNORE_LINES_FOR_CODE_HYGIENE[1] */
extern int main(void);

void _start(void) {
  main();
}

/* LLVM can generate references to memset/memcpy and other intrinsics
 * during optimization (opt) or during translation (llc).
 * Since barebones tests are compiled with -nostdlib those functions
 * are not provided by libc and will re-implement them here
 */

typedef unsigned int size_t;

void *memset(void *s, int c, size_t n) __attribute__((weak));
void *memset(void *s, int c, size_t n) {
  size_t i;
  for (i = 0; i < n; i++)
    ((char*)s)[i] = c;
   return s;
}

void *memcpy(void *dest, const void *src, size_t n) __attribute__((weak));
void *memcpy(void *dest, const void *src, size_t n) {
  size_t i;
  for (i = 0; i < n; i++)
     ((char*)dest)[i] = ((char*)src)[i];
  return dest;
}

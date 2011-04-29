/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * llvm-intrinsics.ll is generated from this file.
 *
 * LLVM can generate references to memset/memcpy and other intrinsics
 * during optimization (opt) or during translation (llc). If libc
 * was included during bitcode linking, then these functions will be
 * provided by libc. Otherwise, if libc is not available, e.g. in
 * the barebones tests, then this file provides definitions that will
 * be used instead.
 *
 * Everything in this file should be declared weak so that the libc
 * versions will take precedence.
 *
 * Re-compile using:
 *
 *  tools/llvm/driver.py -S tools/llvm/llvm-intrinsics.c \
 *    -o tools/llvm/llvm-intrinsics.ll
 *
 * Avoid optimization, because the resulting bitcode may
 * become circular.
 *
 * And make sure llvm-intrinsics.ll begins with:
 *   target triple = "armv7-none-linux-gnueabi"
 * Otherwise, the triple won't match other bitcode files and
 * the wrong target backend might be used during llc.
 * For example, on MacOS, the target backend would default to Darwin,
 * which would cause the wrong type of name mangling to be used.
 *
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


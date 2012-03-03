/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#ifndef __BYTESWAP_H
#define __BYTESWAP_H

#include <stdint.h>

/* Copy byte from position s to position d */
#define __swapbyte(val, s, d)   (((val >> (8*s)) & 0xFF) << (8*d))

static inline
uint16_t __bswap_16(uint16_t x) {
  return __swapbyte(x, 0, 1) | __swapbyte(x, 1, 0);
}

static inline
uint32_t __bswap_32(uint32_t x) {
  return __swapbyte(x, 0, 3) |
         __swapbyte(x, 1, 2) |
         __swapbyte(x, 2, 1) |
         __swapbyte(x, 3, 0);
}

static inline
uint64_t __bswap_64(uint64_t x) {
  return  __swapbyte(x, 0, 7) |
          __swapbyte(x, 1, 6) |
          __swapbyte(x, 2, 5) |
          __swapbyte(x, 3, 4) |
          __swapbyte(x, 4, 3) |
          __swapbyte(x, 5, 2) |
          __swapbyte(x, 6, 1) |
          __swapbyte(x, 7, 0);
}

/* Evaluate the argument once */
#define bswap_16(_x)  (__bswap_16(((uint16_t)(_x))))
#define bswap_32(_x)  (__bswap_32(((uint32_t)(_x))))
#define bswap_64(_x)  (__bswap_64(((uint64_t)(_x))))

#endif

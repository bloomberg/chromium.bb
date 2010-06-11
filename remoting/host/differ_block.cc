// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/differ_block.h"

#include <stdlib.h>

#if !defined(USE_SSE2)
#if defined(__SSE2__) || defined(ARCH_CPU_X86_64) || defined(_MSC_VER)
#define USE_SSE2 1
#else
#define USE_SSE2 0
#endif
#endif

#if USE_SSE2
#include <emmintrin.h>
#endif

namespace remoting {

#if USE_SSE2
int BlockDifference(const uint8* image1, const uint8* image2, int stride) {
  __m128i acc = _mm_setzero_si128();
  __m128i v0;
  __m128i v1;
  __m128i sad;
  for (int y = 0; y < kBlockHeight; ++y) {
    const __m128i* i1 = reinterpret_cast<const __m128i*>(image1);
    const __m128i* i2 = reinterpret_cast<const __m128i*>(image2);
    v0 = _mm_loadu_si128(i1);
    v1 = _mm_loadu_si128(i2);
    sad = _mm_sad_epu8(v0, v1);
    acc = _mm_adds_epu16(acc, sad);
    v0 = _mm_loadu_si128(i1 + 1);
    v1 = _mm_loadu_si128(i2 + 1);
    sad = _mm_sad_epu8(v0, v1);
    acc = _mm_adds_epu16(acc, sad);
    v0 = _mm_loadu_si128(i1 + 2);
    v1 = _mm_loadu_si128(i2 + 2);
    sad = _mm_sad_epu8(v0, v1);
    acc = _mm_adds_epu16(acc, sad);
    v0 = _mm_loadu_si128(i1 + 3);
    v1 = _mm_loadu_si128(i2 + 3);
    sad = _mm_sad_epu8(v0, v1);
    acc = _mm_adds_epu16(acc, sad);
    v0 = _mm_loadu_si128(i1 + 4);
    v1 = _mm_loadu_si128(i2 + 4);
    sad = _mm_sad_epu8(v0, v1);
    acc = _mm_adds_epu16(acc, sad);
    v0 = _mm_loadu_si128(i1 + 5);
    v1 = _mm_loadu_si128(i2 + 5);
    sad = _mm_sad_epu8(v0, v1);
    acc = _mm_adds_epu16(acc, sad);
    v0 = _mm_loadu_si128(i1 + 6);
    v1 = _mm_loadu_si128(i2 + 6);
    sad = _mm_sad_epu8(v0, v1);
    acc = _mm_adds_epu16(acc, sad);
    v0 = _mm_loadu_si128(i1 + 7);
    v1 = _mm_loadu_si128(i2 + 7);
    sad = _mm_sad_epu8(v0, v1);
    acc = _mm_adds_epu16(acc, sad);
    sad = _mm_shuffle_epi32(acc, 0xEE);  // [acc3, acc2, acc3, acc2]
    sad = _mm_adds_epu16(sad, acc);
    int diff = _mm_cvtsi128_si32(sad);
    if (diff) {
      return 1;
    }
    image1 += stride;
    image2 += stride;
  }
  return 0;
}
#else
int BlockDifference(const uint8* image1, const uint8* image2, int stride) {
  // Number of uint64s in each row of the block.
  // This must be an integral number.
  int int64s_per_row = (kBlockWidth * kBytesPerPixel) / sizeof(uint64);

  for (int y = 0; y < kBlockHeight; y++) {
    const uint64* prev = reinterpret_cast<const uint64*>(image1);
    const uint64* curr = reinterpret_cast<const uint64*>(image2);

    // Check each row in uint64-sized chunks.
    // Note that this check may straddle multiple pixels. This is OK because
    // we're interested in identifying whether or not there was change - we
    // don't care what the actual change is.
    for (int x = 0; x < int64s_per_row; x++) {
      if (*prev++ != *curr++) {
        return 1;
      }
    }
    image1 += stride;
    image2 += stride;
  }
  return 0;
}
#endif

}  // namespace remoting

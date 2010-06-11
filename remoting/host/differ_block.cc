// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "differ_block.h"

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

// TODO(fbarchard): Use common header for block size.
const int kBlockWidth = 32;
const int kBlockHeight = 32;
const int kBytesPerPixel = 3;

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
    image1 += stride;
    image2 += stride;
  }
  sad = _mm_shuffle_epi32(acc, 0xEE); // [acc3, acc2, acc3, acc2]
  sad = _mm_adds_epu16(sad, acc);
  int diff = _mm_cvtsi128_si32(sad);
  return diff;
}
#else
int BlockDifference(const uint8* image1, const uint8* image2, int stride) {
  int diff = 0;
  for (int y = 0; y < kBlockHeight; ++y) {
    for (int x = 0; x < kBlockWidth * kBytesPerPixel; ++x) {
      diff += abs(image1[x] - image2[x]);
    }
    image1 += stride;
    image2 += stride;
  }
  return diff;
}
#endif

}  // namespace remoting

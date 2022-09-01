// Auto-generated file. Do not edit!
//   Template: src/f32-vbinary/vopc-sse.c.in
//   Generator: tools/xngen
//
// Copyright 2019 Google LLC
//
// This source code is licensed under the BSD-style license found in the
// LICENSE file in the root directory of this source tree.

#include <assert.h>

#include <xmmintrin.h>

#include <xnnpack/common.h>
#include <xnnpack/intrinsics-polyfill.h>
#include <xnnpack/vbinary.h>


void xnn_f32_vdivc_minmax_ukernel__sse_x4(
    size_t n,
    const float* a,
    const float* b,
    float* y,
    const union xnn_f32_minmax_params params[restrict XNN_MIN_ELEMENTS(1)]) XNN_OOB_READS
{
  assert(n != 0);
  assert(n % sizeof(float) == 0);
  assert(a != NULL);
  assert(b != NULL);
  assert(y != NULL);

  const __m128 vy_min = _mm_load_ps(params->sse.min);
  const __m128 vy_max = _mm_load_ps(params->sse.max);

  const __m128 vb = _mm_load1_ps(b);
  for (; n >= 4 * sizeof(float); n -= 4 * sizeof(float)) {
    const __m128 va0123 = _mm_loadu_ps(a);
    a += 4;

    __m128 vy0123 = _mm_div_ps(va0123, vb);


    vy0123 = _mm_max_ps(vy0123, vy_min);

    vy0123 = _mm_min_ps(vy0123, vy_max);

    _mm_storeu_ps(y, vy0123);
    y += 4;
  }
  if XNN_UNLIKELY(n != 0) {
    const __m128 va0123 = _mm_loadu_ps(a);

    __m128 vy0123 = _mm_div_ps(va0123, vb);
    vy0123 = _mm_max_ps(vy0123, vy_min);
    vy0123 = _mm_min_ps(vy0123, vy_max);
    if (n & (2 * sizeof(float))) {
      _mm_storel_pi((__m64*) y, vy0123);
      vy0123 = _mm_movehl_ps(vy0123, vy0123);
      y += 2;
    }
    if (n & (1 * sizeof(float))) {
      _mm_store_ss(y, vy0123);
    }
  }
}

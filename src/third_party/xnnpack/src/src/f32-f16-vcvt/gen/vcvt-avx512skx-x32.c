// Auto-generated file. Do not edit!
//   Template: src/f32-f16-vcvt/avx512skx.c.in
//   Generator: tools/xngen
//
// Copyright 2021 Google LLC
//
// This source code is licensed under the BSD-style license found in the
// LICENSE file in the root directory of this source tree.

#include <assert.h>

#include <immintrin.h>

#include <xnnpack/common.h>
#include <xnnpack/intrinsics-polyfill.h>
#include <xnnpack/vcvt.h>


void xnn_f32_f16_vcvt_ukernel__avx512skx_x32(
    size_t n,
    const float* input,
    void* output,
    const union xnn_f32_f16_cvt_params params[restrict XNN_MIN_ELEMENTS(1)])
{
  assert(n != 0);
  assert(n % sizeof(float) == 0);
  assert(input != NULL);
  assert(output != NULL);

  uint16_t* o = (uint16_t*) output;
  for (; n >= 32 * sizeof(float); n -= 32 * sizeof(float)) {
    const __m512 vf0 = _mm512_loadu_ps(input);
    const __m512 vf1 = _mm512_loadu_ps(input + 16);
    input += 32;

    _mm256_storeu_si256((__m256i*) o, _mm512_cvtps_ph(vf0, _MM_FROUND_NO_EXC));
    _mm256_storeu_si256((__m256i*) (o + 16), _mm512_cvtps_ph(vf1, _MM_FROUND_NO_EXC));
    o += 32;
  }
  for (; n >= 16 * sizeof(float); n -= 16 * sizeof(float)) {
    const __m512 vf = _mm512_loadu_ps(input);
    input += 16;

    _mm256_storeu_si256((__m256i*) o, _mm512_cvtps_ph(vf, _MM_FROUND_NO_EXC));
    o += 16;
  }
  if XNN_UNLIKELY(n != 0) {
    assert(n >= 1 * sizeof(float));
    assert(n <= 15 * sizeof(float));

    // Prepare mask for valid elements (depends on n).
    n >>= 2 /* log2(sizeof(float)) */;
    const __mmask16 vmask = _cvtu32_mask16((uint16_t) ((uint32_t) (UINT32_C(1) << n) - UINT32_C(1)));

    const __m512 vf = _mm512_maskz_loadu_ps(vmask, input);
    const __m256i vh = _mm512_cvtps_ph(vf, _MM_FROUND_NO_EXC);
    _mm256_mask_storeu_epi16(o, vmask, vh);
  }
}

// Auto-generated file. Do not edit!
//   Template: src/f16-vhswish/f16c.c.in
//   Generator: tools/xngen
//
// Copyright 2022 Google LLC
//
// This source code is licensed under the BSD-style license found in the
// LICENSE file in the root directory of this source tree.

#include <assert.h>

#include <immintrin.h>

#include <xnnpack/common.h>
#include <xnnpack/vunary.h>


void xnn_f16_vhswish_ukernel__f16c_x16(
    size_t n,
    const void* restrict x_ptr,
    void* restrict y_ptr,
    const union xnn_f16_hswish_params params[restrict XNN_MIN_ELEMENTS(1)]) XNN_OOB_READS
{
  assert(n != 0);
  assert(n % sizeof(uint16_t) == 0);

  const uint16_t* x = (const uint16_t*) x_ptr;
  uint16_t* y = (uint16_t*) y_ptr;

  const __m256 vsixth = _mm256_load_ps(params->avx.sixth);
  const __m256 vthree = _mm256_load_ps(params->avx.three);
  const __m128i vsix = _mm_load_si128((const __m128i*) params->avx.six);
  const __m128i vzero = _mm_setzero_si128();

  for (; n >= 16 * sizeof(uint16_t); n -= 16 * sizeof(uint16_t)) {
    __m256 vx01234567 = _mm256_cvtph_ps(_mm_loadu_si128((const __m128i*) x));
    __m256 vx89ABCDEF = _mm256_cvtph_ps(_mm_loadu_si128((const __m128i*) (x + 8)));
    x += 16;

    __m128i vacc01234567 = _mm256_cvtps_ph(_mm256_add_ps(vx01234567, vthree), _MM_FROUND_NO_EXC);
    vx01234567 = _mm256_cvtph_ps(_mm256_cvtps_ph(_mm256_mul_ps(vx01234567, vsixth), _MM_FROUND_NO_EXC));
    __m128i vacc89ABCDEF = _mm256_cvtps_ph(_mm256_add_ps(vx89ABCDEF, vthree), _MM_FROUND_NO_EXC);
    vx89ABCDEF = _mm256_cvtph_ps(_mm256_cvtps_ph(_mm256_mul_ps(vx89ABCDEF, vsixth), _MM_FROUND_NO_EXC));

    vacc01234567 = _mm_max_epi16(vacc01234567, vzero);
    vacc89ABCDEF = _mm_max_epi16(vacc89ABCDEF, vzero);

    vacc01234567 = _mm_min_epi16(vacc01234567, vsix);
    vacc89ABCDEF = _mm_min_epi16(vacc89ABCDEF, vsix);

    vacc01234567 = _mm256_cvtps_ph(_mm256_mul_ps(_mm256_cvtph_ps(vacc01234567), vx01234567), _MM_FROUND_NO_EXC);
    vacc89ABCDEF = _mm256_cvtps_ph(_mm256_mul_ps(_mm256_cvtph_ps(vacc89ABCDEF), vx89ABCDEF), _MM_FROUND_NO_EXC);

    _mm_storeu_si128((__m128i*) y, vacc01234567);
    _mm_storeu_si128((__m128i*) (y + 8), vacc89ABCDEF);
    y += 16;
  }
  for (; n >= 8 * sizeof(uint16_t); n -= 8 * sizeof(uint16_t)) {
    __m256 vx = _mm256_cvtph_ps(_mm_loadu_si128((const __m128i*) x));
    x += 8;
    __m128i vacc = _mm256_cvtps_ph(_mm256_add_ps(vx, vthree), _MM_FROUND_NO_EXC);
    vx = _mm256_cvtph_ps(_mm256_cvtps_ph(_mm256_mul_ps(vx, vsixth), _MM_FROUND_NO_EXC));
    vacc = _mm_max_epi16(vacc, vzero);
    vacc = _mm_min_epi16(vacc, vsix);
    vacc = _mm256_cvtps_ph(_mm256_mul_ps(_mm256_cvtph_ps(vacc), vx), _MM_FROUND_NO_EXC);
    _mm_storeu_si128((__m128i*) y, vacc);
    y += 8;
  }
  if XNN_UNLIKELY(n != 0) {
    __m256 vx = _mm256_cvtph_ps(_mm_loadu_si128((const __m128i*) x));
    __m128i vacc = _mm256_cvtps_ph(_mm256_add_ps(vx, vthree), _MM_FROUND_NO_EXC);
    vx = _mm256_cvtph_ps(_mm256_cvtps_ph(_mm256_mul_ps(vx, vsixth), _MM_FROUND_NO_EXC));
    vacc = _mm_max_epi16(vacc, vzero);
    vacc = _mm_min_epi16(vacc, vsix);
    vacc = _mm256_cvtps_ph(_mm256_mul_ps(_mm256_cvtph_ps(vacc), vx), _MM_FROUND_NO_EXC);

    if (n & (4 * sizeof(uint16_t))) {
      _mm_storel_epi64((__m128i*) y, vacc);
      vacc = _mm_unpackhi_epi64(vacc, vacc);
      y += 4;
    }
    if (n & (2 * sizeof(uint16_t))) {
      *((uint32_t*) y) = (uint32_t) _mm_cvtsi128_si32(vacc);
      vacc = _mm_srli_epi64(vacc, 32);
      y += 2;
    }
    if (n & (1 * sizeof(uint16_t))) {
      *y = (uint16_t) _mm_extract_epi16(vacc, 0);
    }
  }
}

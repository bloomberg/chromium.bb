// Auto-generated file. Do not edit!
//   Template: src/f16-vhswish/neonfp16arith.c.in
//   Generator: tools/xngen
//
// Copyright 2020 Google LLC
//
// This source code is licensed under the BSD-style license found in the
// LICENSE file in the root directory of this source tree.

#include <assert.h>

#include <arm_neon.h>

#include <xnnpack/common.h>
#include <xnnpack/vunary.h>


void xnn_f16_vhswish_ukernel__neonfp16arith_x16(
    size_t n,
    const void* restrict x_ptr,
    void* restrict y_ptr,
    const union xnn_f16_hswish_params params[restrict XNN_MIN_ELEMENTS(1)]) XNN_OOB_READS
{
  assert(n != 0);
  assert(n % sizeof(__fp16) == 0);

  const __fp16* x = (const __fp16*) x_ptr;
  __fp16* y = (__fp16*) y_ptr;

  const float16x8_t vsixth = vreinterpretq_f16_u16(vld1q_dup_u16(&params->neon.sixth));
  const float16x8_t vthree = vreinterpretq_f16_u16(vld1q_dup_u16(&params->neon.three));
  const int16x8_t vsix = vreinterpretq_s16_u16(vld1q_dup_u16(&params->neon.six));
  const int16x8_t vzero = vdupq_n_s16(0);

  for (; n >= 16 * sizeof(__fp16); n -= 16 * sizeof(__fp16)) {
    float16x8_t vx01234567 = vld1q_f16(x); x += 8;
    float16x8_t vx89ABCDEF = vld1q_f16(x); x += 8;

    float16x8_t vacc01234567 = vaddq_f16(vx01234567, vthree);
    vx01234567 = vmulq_f16(vx01234567, vsixth);
    float16x8_t vacc89ABCDEF = vaddq_f16(vx89ABCDEF, vthree);
    vx89ABCDEF = vmulq_f16(vx89ABCDEF, vsixth);

    vacc01234567 = vreinterpretq_f16_s16(vmaxq_s16(vreinterpretq_s16_f16(vacc01234567), vzero));
    vacc89ABCDEF = vreinterpretq_f16_s16(vmaxq_s16(vreinterpretq_s16_f16(vacc89ABCDEF), vzero));

    vacc01234567 = vreinterpretq_f16_s16(vminq_s16(vreinterpretq_s16_f16(vacc01234567), vsix));
    vacc89ABCDEF = vreinterpretq_f16_s16(vminq_s16(vreinterpretq_s16_f16(vacc89ABCDEF), vsix));

    vacc01234567 = vmulq_f16(vacc01234567, vx01234567);
    vacc89ABCDEF = vmulq_f16(vacc89ABCDEF, vx89ABCDEF);

    vst1q_f16(y, vacc01234567); y += 8;
    vst1q_f16(y, vacc89ABCDEF); y += 8;
  }
  for (; n >= 8 * sizeof(__fp16); n -= 8 * sizeof(__fp16)) {
    float16x8_t vx = vld1q_f16(x); x += 8;
    float16x8_t vacc = vaddq_f16(vx, vthree);
    vx = vmulq_f16(vx, vsixth);
    vacc = vreinterpretq_f16_s16(vmaxq_s16(vreinterpretq_s16_f16(vacc), vzero));
    vacc = vreinterpretq_f16_s16(vminq_s16(vreinterpretq_s16_f16(vacc), vsix));
    vacc = vmulq_f16(vacc, vx);
    vst1q_f16(y, vacc); y += 8;
  }
  if XNN_UNLIKELY(n != 0) {
    float16x8_t vx = vld1q_f16(x);
    float16x8_t vacc = vaddq_f16(vx, vthree);
    vx = vmulq_f16(vx, vsixth);
    vacc = vreinterpretq_f16_s16(vmaxq_s16(vreinterpretq_s16_f16(vacc), vzero));
    vacc = vreinterpretq_f16_s16(vminq_s16(vreinterpretq_s16_f16(vacc), vsix));
    vacc = vmulq_f16(vacc, vx);

    float16x4_t vacc_lo = vget_low_f16(vacc);
    if (n & (4 * sizeof(__fp16))) {
      vst1_f16(y, vacc_lo); y += 4;
      vacc_lo = vget_high_f16(vacc);
    }
    if (n & (2 * sizeof(__fp16))) {
      vst1_lane_u32((void*) y, vreinterpret_u32_f16(vacc_lo), 0); y += 2;
      vacc_lo = vext_f16(vacc_lo, vacc_lo, 2);
    }
    if (n & (1 * sizeof(__fp16))) {
      vst1_lane_f16(y, vacc_lo, 0);
    }
  }
}

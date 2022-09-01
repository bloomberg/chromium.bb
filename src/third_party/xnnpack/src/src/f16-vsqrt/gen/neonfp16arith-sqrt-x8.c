// Auto-generated file. Do not edit!
//   Template: src/f16-vsqrt/neonfp16arith-sqrt.c.in
//   Generator: tools/xngen
//
// Copyright 2022 Google LLC
//
// This source code is licensed under the BSD-style license found in the
// LICENSE file in the root directory of this source tree.

#include <assert.h>

#include <arm_neon.h>

#include <xnnpack/common.h>
#include <xnnpack/vunary.h>


void xnn_f16_vsqrt_ukernel__neonfp16arith_sqrt_x8(
    size_t n,
    const void* input,
    void* output,
    const union xnn_f16_sqrt_params params[restrict XNN_MIN_ELEMENTS(1)]) XNN_OOB_READS
{
  assert(n != 0);
  assert(n % sizeof(__fp16) == 0);
  assert(input != NULL);
  assert(output != NULL);

  const __fp16* i = (const __fp16*) input;
  __fp16* o = (__fp16*) output;
  for (; n >= 8 * sizeof(__fp16); n -= 8 * sizeof(__fp16)) {
    float16x8_t vacc = vld1q_f16(i); i += 8;
    vacc = vsqrtq_f16(vacc);
    vst1q_f16(o, vacc); o += 8;
  }
  if XNN_UNLIKELY(n != 0) {
    const float16x8_t vacc = vld1q_f16(i);
    float16x4_t vacc_lo = vsqrt_f16(vget_low_f16(vacc));
    if (n & (4 * sizeof(__fp16))) {
      vst1_f16(o, vacc_lo); o += 4;
      vacc_lo = vsqrt_f16(vget_high_f16(vacc));
    }
    if (n & (2 * sizeof(__fp16))) {
      vst1_lane_u32((void*) o, vreinterpret_u32_f16(vacc_lo), 0); o += 2;
      vacc_lo = vext_f16(vacc_lo, vacc_lo, 2);
    }
    if (n & (1 * sizeof(__fp16))) {
      vst1_lane_f16(o, vacc_lo, 0);
    }
  }
}

// Auto-generated file. Do not edit!
//   Template: src/qu8-igemm/c4-neondot.c.in
//   Generator: tools/xngen
//
// Copyright 2020 Google LLC
//
// This source code is licensed under the BSD-style license found in the
// LICENSE file in the root directory of this source tree.

#include <assert.h>

#include <arm_neon.h>

#include <xnnpack/igemm.h>
#include <xnnpack/math.h>


void xnn_qu8_igemm_minmax_rndnu_ukernel_1x8c4__neondot(
    size_t mr,
    size_t nc,
    size_t kc,
    size_t ks,
    const uint8_t** restrict a,
    const void* restrict w,
    uint8_t* restrict c,
    size_t cm_stride,
    size_t cn_stride,
    size_t a_offset,
    const uint8_t* zero,
    const union xnn_qu8_conv_minmax_params params[restrict XNN_MIN_ELEMENTS(1)]) XNN_OOB_READS
{
  assert(mr != 0);
  assert(mr <= 1);
  assert(nc != 0);
  assert(kc != 0);
  assert(ks != 0);
  assert(ks % (1 * sizeof(void*)) == 0);
  assert(a_offset % sizeof(uint8_t) == 0);
  assert(a != NULL);
  assert(w != NULL);
  assert(c != NULL);

  kc = round_up_po2(kc, 4 * sizeof(uint8_t));
  uint8_t* c0 = c;

  const uint8x8_t va_zero_point = vld1_dup_u8(&params->rndnu_neon.kernel_zero_point[0]);

  do {
    // Initialize accumulators with bias. 8 bias values are loaded from the
    // weight matrix, at the start of the group of 8 columns.
    uint32x4_t vpacc0x0123 = vld1q_u32(w); w = (const void*) ((const uint32_t*) w + 4);
    uint32x4_t vpacc0x4567 = vld1q_u32(w); w = (const void*) ((const uint32_t*) w + 4);
    uint32x2_t vnacc0 = vmov_n_u32(0);

    size_t p = ks;
    do {
      const uint8_t* restrict a0 = a[0];
      if XNN_UNPREDICTABLE(a0 != zero) {
        a0 = (const uint8_t*) ((uintptr_t) a0 + a_offset);
      }
      a += 1;

      // Inner accumulation loop along the 8 columns.
      size_t k = kc;
      // 2x partial unrolled loop to load 8 bytes at a time.
      while (k >= 8 * sizeof(uint8_t)) {
        // Load a 1x8 block of activations.
        const uint8x8_t va0x01234567 = vld1_u8(a0); a0 += 8;

        // Load a 8x8 block of weights.
        const uint8x16_t vb0123x0123 = vld1q_u8(w); w = (const void*) ((const uint8_t*) w + 16);
        const uint8x16_t vb0123x4567 = vld1q_u8(w); w = (const void*) ((const uint8_t*) w + 16);
        const uint8x16_t vb4567x0123 = vld1q_u8(w); w = (const void*) ((const uint8_t*) w + 16);
        const uint8x16_t vb4567x4567 = vld1q_u8(w); w = (const void*) ((const uint8_t*) w + 16);

        // Multiply-accumulate: 1x8 * 8x8 --> 1x8.
        vnacc0 = vdot_u32(vnacc0, va_zero_point, va0x01234567);
        vpacc0x0123 = vdotq_lane_u32(vpacc0x0123, vb0123x0123, va0x01234567, 0);
        vpacc0x4567 = vdotq_lane_u32(vpacc0x4567, vb0123x4567, va0x01234567, 0);
        vpacc0x0123 = vdotq_lane_u32(vpacc0x0123, vb4567x0123, va0x01234567, 1);
        vpacc0x4567 = vdotq_lane_u32(vpacc0x4567, vb4567x4567, va0x01234567, 1);

        k -= 8 * sizeof(uint8_t);
      }
      // Handle up to 4 final positions of `k`
      if XNN_UNLIKELY(k != 0) {
        // Load a 1x4 block of activations.
        const uint8x8_t va0x01234567 = vreinterpret_u8_u32(vld1_lane_u32((const void*) a0, vmov_n_u32(0), 0)); a0 += 4;

        // Load a 4x8 block of weights.
        const uint8x16_t vb0123x0123 = vld1q_u8(w); w = (const void*) ((const uint8_t*) w + 16);
        const uint8x16_t vb0123x4567 = vld1q_u8(w); w = (const void*) ((const uint8_t*) w + 16);

        // Multiply-accumulate: 1x4 * 4x8 --> 1x8.
        vnacc0 = vdot_u32(vnacc0, va_zero_point, va0x01234567);
        vpacc0x0123 = vdotq_lane_u32(vpacc0x0123, vb0123x0123, va0x01234567, 0);
        vpacc0x4567 = vdotq_lane_u32(vpacc0x4567, vb0123x4567, va0x01234567, 0);
      }
      p -= 1 * sizeof(void*);
    } while (p != 0);

    // Subtract zero point from accumulators.
    vnacc0 = vpadd_u32(vnacc0, vnacc0);
    const uint32x4_t vnacc0x0123 = vcombine_u32(vnacc0, vnacc0);
    int32x4_t vacc0x0123 = vreinterpretq_s32_u32(vsubq_u32(vpacc0x0123, vnacc0x0123));
    int32x4_t vacc0x4567 = vreinterpretq_s32_u32(vsubq_u32(vpacc0x4567, vnacc0x0123));

    const int32x4_t vright_pre_shift = vld1q_dup_s32(&params->rndnu_neon.right_pre_shift);
    const int32x4_t vmultiplier = vld1q_dup_s32(&params->rndnu_neon.multiplier);
    const int32x4_t vright_post_shift = vld1q_dup_s32(&params->rndnu_neon.right_post_shift);

    vacc0x0123 = vshlq_s32(vacc0x0123, vright_pre_shift);
    vacc0x4567 = vshlq_s32(vacc0x4567, vright_pre_shift);

    vacc0x0123 = vqdmulhq_s32(vacc0x0123, vmultiplier);
    vacc0x4567 = vqdmulhq_s32(vacc0x4567, vmultiplier);

    vacc0x0123 = vrshlq_s32(vacc0x0123, vright_post_shift);
    vacc0x4567 = vrshlq_s32(vacc0x4567, vright_post_shift);

    const int16x8_t voutput_zero_point = vld1q_dup_s16(&params->rndnu_neon.output_zero_point);
#if XNN_ARCH_ARM64
    const int16x8_t vacc0x01234567 = vqaddq_s16(vqmovn_high_s32(vqmovn_s32(vacc0x0123), vacc0x4567), voutput_zero_point);

    uint8x8_t vout0x01234567 = vqmovun_s16(vacc0x01234567);
#else
    const int16x8_t vacc0x01234567 = vqaddq_s16(vcombine_s16(vqmovn_s32(vacc0x0123), vqmovn_s32(vacc0x4567)), voutput_zero_point);

    uint8x8_t vout0x01234567 = vqmovun_s16(vacc0x01234567);
#endif
    const uint8x8_t voutput_min = vld1_dup_u8(&params->rndnu_neon.output_min);
    const uint8x8_t voutput_max = vld1_dup_u8(&params->rndnu_neon.output_max);

    vout0x01234567 = vmax_u8(vout0x01234567, voutput_min);

    vout0x01234567 = vmin_u8(vout0x01234567, voutput_max);

    if (nc >= 8) {
      vst1_u8(c0 + 0, vout0x01234567);

      c0 = (uint8_t*) ((uintptr_t) c0 + cn_stride);

      a = (const uint8_t**restrict) ((uintptr_t) a - ks);

      nc -= 8;
    } else {
      if (nc & 4) {
        vst1_lane_u32((void*) c0, vreinterpret_u32_u8(vout0x01234567), 0); c0 += 4;
        vout0x01234567 = vext_u8(vout0x01234567, vout0x01234567, 4);
      }
      if (nc & 2) {
        vst1_lane_u16((void*) c0, vreinterpret_u16_u8(vout0x01234567), 0); c0 += 2;
        vout0x01234567 = vext_u8(vout0x01234567, vout0x01234567, 2);
      }
      if (nc & 1) {
        vst1_lane_u8(c0, vout0x01234567, 0);
      }

      nc = 0;
    }
  } while (nc != 0);
}

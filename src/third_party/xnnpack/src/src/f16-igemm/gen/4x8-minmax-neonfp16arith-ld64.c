// Auto-generated file. Do not edit!
//   Template: src/f16-igemm/neonfp16arith-ld64.c.in
//   Generator: tools/xngen
//
// Copyright 2019 Google LLC
//
// This source code is licensed under the BSD-style license found in the
// LICENSE file in the root directory of this source tree.


#include <assert.h>

#include <arm_neon.h>

#include <xnnpack/igemm.h>


void xnn_f16_igemm_minmax_ukernel_4x8__neonfp16arith_ld64(
    size_t mr,
    size_t nc,
    size_t kc,
    size_t ks,
    const void** restrict a,
    const void* restrict w,
    void* restrict c,
    size_t cm_stride,
    size_t cn_stride,
    size_t a_offset,
    const void* zero,
    const union xnn_f16_minmax_params params[restrict XNN_MIN_ELEMENTS(1)])
{
  assert(mr != 0);
  assert(mr <= 4);
  assert(nc != 0);
  assert(kc != 0);
  assert(kc % sizeof(__fp16) == 0);
  assert(ks != 0);
  assert(ks % (4 * sizeof(void*)) == 0);
  assert(a_offset % sizeof(__fp16) == 0);
  assert(a != NULL);
  assert(w != NULL);
  assert(c != NULL);

  __fp16* c0 = (__fp16*) c;
  __fp16* c1 = (__fp16*) ((uintptr_t) c0 + cm_stride);
  if XNN_UNPREDICTABLE(mr < 2) {
    c1 = c0;
  }
  __fp16* c2 = (__fp16*) ((uintptr_t) c1 + cm_stride);
  if XNN_UNPREDICTABLE(mr <= 2) {
    c2 = c1;
  }
  __fp16* c3 = (__fp16*) ((uintptr_t) c2 + cm_stride);
  if XNN_UNPREDICTABLE(mr != 4) {
    c3 = c2;
  }

  do {
    float16x8_t vacc0x01234567 = vld1q_f16(w); w = (const void*) ((uintptr_t) w + sizeof(float16x8_t));
    float16x8_t vacc1x01234567 = vacc0x01234567;
    float16x8_t vacc2x01234567 = vacc0x01234567;
    float16x8_t vacc3x01234567 = vacc0x01234567;

    size_t p = ks;
    do {
      const __fp16* restrict a0 = (const __fp16*) a[0];
      assert(a0 != NULL);
      if XNN_UNPREDICTABLE(a0 != zero) {
        a0 = (const __fp16*) ((uintptr_t) a0 + a_offset);
      }
      const __fp16* restrict a1 = (const __fp16*) a[1];
      assert(a1 != NULL);
      if XNN_UNPREDICTABLE(a1 != zero) {
        a1 = (const __fp16*) ((uintptr_t) a1 + a_offset);
      }
      const __fp16* restrict a2 = (const __fp16*) a[2];
      assert(a2 != NULL);
      if XNN_UNPREDICTABLE(a2 != zero) {
        a2 = (const __fp16*) ((uintptr_t) a2 + a_offset);
      }
      const __fp16* restrict a3 = (const __fp16*) a[3];
      assert(a3 != NULL);
      if XNN_UNPREDICTABLE(a3 != zero) {
        a3 = (const __fp16*) ((uintptr_t) a3 + a_offset);
      }
      a += 4;

      size_t k = kc;
      for (; k >= 4 * sizeof(__fp16); k -= 4 * sizeof(__fp16)) {
        const float16x4_t va0 = vld1_f16(a0); a0 += 4;
        const float16x4_t va1 = vld1_f16(a1); a1 += 4;
        const float16x4_t va2 = vld1_f16(a2); a2 += 4;
        const float16x4_t va3 = vld1_f16(a3); a3 += 4;

        const float16x8_t vb01234567c0 = vld1q_f16(w); w = (const void*) ((uintptr_t) w + sizeof(float16x8_t));

        #if XNN_ARCH_ARM64
          vacc0x01234567 = vfmaq_lane_f16(vacc0x01234567, vb01234567c0, va0, 0);
          vacc1x01234567 = vfmaq_lane_f16(vacc1x01234567, vb01234567c0, va1, 0);
          vacc2x01234567 = vfmaq_lane_f16(vacc2x01234567, vb01234567c0, va2, 0);
          vacc3x01234567 = vfmaq_lane_f16(vacc3x01234567, vb01234567c0, va3, 0);
        #else
          const float16x8_t va0c0 = vdupq_lane_f16(va0, 0);
          const float16x8_t va1c0 = vdupq_lane_f16(va1, 0);
          const float16x8_t va2c0 = vdupq_lane_f16(va2, 0);
          const float16x8_t va3c0 = vdupq_lane_f16(va3, 0);

          vacc0x01234567 = vfmaq_f16(vacc0x01234567, va0c0, vb01234567c0);
          vacc1x01234567 = vfmaq_f16(vacc1x01234567, va1c0, vb01234567c0);
          vacc2x01234567 = vfmaq_f16(vacc2x01234567, va2c0, vb01234567c0);
          vacc3x01234567 = vfmaq_f16(vacc3x01234567, va3c0, vb01234567c0);
        #endif
        const float16x8_t vb01234567c1 = vld1q_f16(w); w = (const void*) ((uintptr_t) w + sizeof(float16x8_t));

        #if XNN_ARCH_ARM64
          vacc0x01234567 = vfmaq_lane_f16(vacc0x01234567, vb01234567c1, va0, 1);
          vacc1x01234567 = vfmaq_lane_f16(vacc1x01234567, vb01234567c1, va1, 1);
          vacc2x01234567 = vfmaq_lane_f16(vacc2x01234567, vb01234567c1, va2, 1);
          vacc3x01234567 = vfmaq_lane_f16(vacc3x01234567, vb01234567c1, va3, 1);
        #else
          const float16x8_t va0c1 = vdupq_lane_f16(va0, 1);
          const float16x8_t va1c1 = vdupq_lane_f16(va1, 1);
          const float16x8_t va2c1 = vdupq_lane_f16(va2, 1);
          const float16x8_t va3c1 = vdupq_lane_f16(va3, 1);

          vacc0x01234567 = vfmaq_f16(vacc0x01234567, va0c1, vb01234567c1);
          vacc1x01234567 = vfmaq_f16(vacc1x01234567, va1c1, vb01234567c1);
          vacc2x01234567 = vfmaq_f16(vacc2x01234567, va2c1, vb01234567c1);
          vacc3x01234567 = vfmaq_f16(vacc3x01234567, va3c1, vb01234567c1);
        #endif
        const float16x8_t vb01234567c2 = vld1q_f16(w); w = (const void*) ((uintptr_t) w + sizeof(float16x8_t));

        #if XNN_ARCH_ARM64
          vacc0x01234567 = vfmaq_lane_f16(vacc0x01234567, vb01234567c2, va0, 2);
          vacc1x01234567 = vfmaq_lane_f16(vacc1x01234567, vb01234567c2, va1, 2);
          vacc2x01234567 = vfmaq_lane_f16(vacc2x01234567, vb01234567c2, va2, 2);
          vacc3x01234567 = vfmaq_lane_f16(vacc3x01234567, vb01234567c2, va3, 2);
        #else
          const float16x8_t va0c2 = vdupq_lane_f16(va0, 2);
          const float16x8_t va1c2 = vdupq_lane_f16(va1, 2);
          const float16x8_t va2c2 = vdupq_lane_f16(va2, 2);
          const float16x8_t va3c2 = vdupq_lane_f16(va3, 2);

          vacc0x01234567 = vfmaq_f16(vacc0x01234567, va0c2, vb01234567c2);
          vacc1x01234567 = vfmaq_f16(vacc1x01234567, va1c2, vb01234567c2);
          vacc2x01234567 = vfmaq_f16(vacc2x01234567, va2c2, vb01234567c2);
          vacc3x01234567 = vfmaq_f16(vacc3x01234567, va3c2, vb01234567c2);
        #endif
        const float16x8_t vb01234567c3 = vld1q_f16(w); w = (const void*) ((uintptr_t) w + sizeof(float16x8_t));

        #if XNN_ARCH_ARM64
          vacc0x01234567 = vfmaq_lane_f16(vacc0x01234567, vb01234567c3, va0, 3);
          vacc1x01234567 = vfmaq_lane_f16(vacc1x01234567, vb01234567c3, va1, 3);
          vacc2x01234567 = vfmaq_lane_f16(vacc2x01234567, vb01234567c3, va2, 3);
          vacc3x01234567 = vfmaq_lane_f16(vacc3x01234567, vb01234567c3, va3, 3);
        #else
          const float16x8_t va0c3 = vdupq_lane_f16(va0, 3);
          const float16x8_t va1c3 = vdupq_lane_f16(va1, 3);
          const float16x8_t va2c3 = vdupq_lane_f16(va2, 3);
          const float16x8_t va3c3 = vdupq_lane_f16(va3, 3);

          vacc0x01234567 = vfmaq_f16(vacc0x01234567, va0c3, vb01234567c3);
          vacc1x01234567 = vfmaq_f16(vacc1x01234567, va1c3, vb01234567c3);
          vacc2x01234567 = vfmaq_f16(vacc2x01234567, va2c3, vb01234567c3);
          vacc3x01234567 = vfmaq_f16(vacc3x01234567, va3c3, vb01234567c3);
        #endif
      }
      if XNN_UNLIKELY(k != 0) {
        do {
          const float16x8_t va0 = vld1q_dup_f16(a0); a0 += 1;
          const float16x8_t va1 = vld1q_dup_f16(a1); a1 += 1;
          const float16x8_t va2 = vld1q_dup_f16(a2); a2 += 1;
          const float16x8_t va3 = vld1q_dup_f16(a3); a3 += 1;

          const float16x8_t vb01234567 = vld1q_f16(w); w = (const void*) ((uintptr_t) w + sizeof(float16x8_t));

          vacc0x01234567 = vfmaq_f16(vacc0x01234567, va0, vb01234567);
          vacc1x01234567 = vfmaq_f16(vacc1x01234567, va1, vb01234567);
          vacc2x01234567 = vfmaq_f16(vacc2x01234567, va2, vb01234567);
          vacc3x01234567 = vfmaq_f16(vacc3x01234567, va3, vb01234567);

          k -= sizeof(__fp16);
        } while (k != 0);
      }
      p -= 4 * sizeof(void*);
    } while (p != 0);


    const float16x8_t vmax = vreinterpretq_f16_u16(vld1q_dup_u16(&params->neon.max));
    vacc0x01234567 = vminq_f16(vacc0x01234567, vmax);
    vacc1x01234567 = vminq_f16(vacc1x01234567, vmax);
    vacc2x01234567 = vminq_f16(vacc2x01234567, vmax);
    vacc3x01234567 = vminq_f16(vacc3x01234567, vmax);

    const float16x8_t vmin = vreinterpretq_f16_u16(vld1q_dup_u16(&params->neon.min));
    vacc0x01234567 = vmaxq_f16(vacc0x01234567, vmin);
    vacc1x01234567 = vmaxq_f16(vacc1x01234567, vmin);
    vacc2x01234567 = vmaxq_f16(vacc2x01234567, vmin);
    vacc3x01234567 = vmaxq_f16(vacc3x01234567, vmin);

    if XNN_LIKELY(nc >= 8) {
      vst1q_f16(c3, vacc3x01234567);
      c3 = (__fp16*) ((uintptr_t) c3 + cn_stride);
      vst1q_f16(c2, vacc2x01234567);
      c2 = (__fp16*) ((uintptr_t) c2 + cn_stride);
      vst1q_f16(c1, vacc1x01234567);
      c1 = (__fp16*) ((uintptr_t) c1 + cn_stride);
      vst1q_f16(c0, vacc0x01234567);
      c0 = (__fp16*) ((uintptr_t) c0 + cn_stride);

      a = (const void**restrict) ((uintptr_t) a - ks);
      nc -= 8;
    } else {
      float16x4_t vacc3x0123 = vget_low_f16(vacc3x01234567);
      float16x4_t vacc2x0123 = vget_low_f16(vacc2x01234567);
      float16x4_t vacc1x0123 = vget_low_f16(vacc1x01234567);
      float16x4_t vacc0x0123 = vget_low_f16(vacc0x01234567);
      if (nc & 4) {
        vst1_f16(c3, vacc3x0123); c3 += 4;
        vst1_f16(c2, vacc2x0123); c2 += 4;
        vst1_f16(c1, vacc1x0123); c1 += 4;
        vst1_f16(c0, vacc0x0123); c0 += 4;

        vacc3x0123 = vget_high_f16(vacc3x01234567);
        vacc2x0123 = vget_high_f16(vacc2x01234567);
        vacc1x0123 = vget_high_f16(vacc1x01234567);
        vacc0x0123 = vget_high_f16(vacc0x01234567);
      }
      if (nc & 2) {
        vst1_lane_u32((void*) c3, vreinterpret_u32_f16(vacc3x0123), 0); c3 += 2;
        vst1_lane_u32((void*) c2, vreinterpret_u32_f16(vacc2x0123), 0); c2 += 2;
        vst1_lane_u32((void*) c1, vreinterpret_u32_f16(vacc1x0123), 0); c1 += 2;
        vst1_lane_u32((void*) c0, vreinterpret_u32_f16(vacc0x0123), 0); c0 += 2;

        vacc3x0123 = vext_f16(vacc3x0123, vacc3x0123, 2);
        vacc2x0123 = vext_f16(vacc2x0123, vacc2x0123, 2);
        vacc1x0123 = vext_f16(vacc1x0123, vacc1x0123, 2);
        vacc0x0123 = vext_f16(vacc0x0123, vacc0x0123, 2);
      }
      if (nc & 1) {
        vst1_lane_f16(c3, vacc3x0123, 0);
        vst1_lane_f16(c2, vacc2x0123, 0);
        vst1_lane_f16(c1, vacc1x0123, 0);
        vst1_lane_f16(c0, vacc0x0123, 0);
      }

      nc = 0;
    }
  } while (nc != 0);
}

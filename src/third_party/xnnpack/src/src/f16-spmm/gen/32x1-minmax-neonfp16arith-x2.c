// Auto-generated file. Do not edit!
//   Template: src/f16-spmm/neonfp16arith.c.in
//   Generator: tools/xngen
//
// Copyright 2019 Google LLC
//
// This source code is licensed under the BSD-style license found in the
// LICENSE file in the root directory of this source tree.

#include <assert.h>

#include <arm_neon.h>

#include <xnnpack/spmm.h>


void xnn_f16_spmm_minmax_ukernel_32x1__neonfp16arith_x2(
    size_t mc,
    size_t nc,
    const void*restrict input,
    const void*restrict weights,
    const int32_t*restrict widx_dmap,
    const uint32_t*restrict nidx_nnzmap,
    void*restrict output,
    size_t output_stride,
    const union xnn_f16_minmax_params params[restrict XNN_MIN_ELEMENTS(1)])
{
  assert(mc != 0);
  assert(mc % sizeof(__fp16) == 0);
  assert(nc != 0);

  const __fp16*restrict i = (const __fp16*) input;
  __fp16*restrict o = (__fp16*) output;

  const float16x8_t vmax = vreinterpretq_f16_u16(vld1q_dup_u16(&params->neon.max));
  const float16x8_t vmin = vreinterpretq_f16_u16(vld1q_dup_u16(&params->neon.min));

  size_t output_decrement = output_stride * nc - 32 * sizeof(__fp16);
  while XNN_LIKELY(mc >= 32 * sizeof(__fp16)) {
    const __fp16*restrict w = (const __fp16*) weights;
    const int32_t* dmap = widx_dmap;
    const uint32_t* nnzmap = nidx_nnzmap;
    size_t n = nc;
    do {
      uint32_t nnz = *nnzmap++;
      float16x8_t vacc01234567x0 = vld1q_dup_f16(w); w += 1;
      float16x8_t vacc01234567x1 = vmovq_n_f16(0.0f);
      float16x8_t vacc89ABCDEFx0 = vacc01234567x0;
      float16x8_t vacc89ABCDEFx1 = vmovq_n_f16(0.0f);
      float16x8_t vaccGHIJKLMNx0 = vacc01234567x0;
      float16x8_t vaccGHIJKLMNx1 = vmovq_n_f16(0.0f);
      float16x8_t vaccOPQRSTUVx0 = vacc01234567x0;
      float16x8_t vaccOPQRSTUVx1 = vmovq_n_f16(0.0f);
      for (; nnz >= 2; nnz -= 2) {
        const intptr_t diff0 = dmap[0];
        const intptr_t diff1 = dmap[1];
        dmap += 2;
        const float16x8_t va01234567x0 = vld1q_f16(i);
        const float16x8_t va89ABCDEFx0 = vld1q_f16(i + 8);
        const float16x8_t vaGHIJKLMNx0 = vld1q_f16(i + 16);
        const float16x8_t vaOPQRSTUVx0 = vld1q_f16(i + 24);
        i = (const __fp16*restrict) ((uintptr_t) i + (uintptr_t) diff0);
        const float16x8_t vb0 = vld1q_dup_f16(w); w += 1;
        vacc01234567x0 = vfmaq_f16(vacc01234567x0, va01234567x0, vb0);
        vacc89ABCDEFx0 = vfmaq_f16(vacc89ABCDEFx0, va89ABCDEFx0, vb0);
        vaccGHIJKLMNx0 = vfmaq_f16(vaccGHIJKLMNx0, vaGHIJKLMNx0, vb0);
        vaccOPQRSTUVx0 = vfmaq_f16(vaccOPQRSTUVx0, vaOPQRSTUVx0, vb0);
        const float16x8_t va01234567x1 = vld1q_f16(i);
        const float16x8_t va89ABCDEFx1 = vld1q_f16(i + 8);
        const float16x8_t vaGHIJKLMNx1 = vld1q_f16(i + 16);
        const float16x8_t vaOPQRSTUVx1 = vld1q_f16(i + 24);
        i = (const __fp16*restrict) ((uintptr_t) i + (uintptr_t) diff1);
        const float16x8_t vb1 = vld1q_dup_f16(w); w += 1;
        vacc01234567x1 = vfmaq_f16(vacc01234567x1, va01234567x1, vb1);
        vacc89ABCDEFx1 = vfmaq_f16(vacc89ABCDEFx1, va89ABCDEFx1, vb1);
        vaccGHIJKLMNx1 = vfmaq_f16(vaccGHIJKLMNx1, vaGHIJKLMNx1, vb1);
        vaccOPQRSTUVx1 = vfmaq_f16(vaccOPQRSTUVx1, vaOPQRSTUVx1, vb1);
      }
      float16x8_t vacc01234567 = vacc01234567x0;
      float16x8_t vacc89ABCDEF = vacc89ABCDEFx0;
      float16x8_t vaccGHIJKLMN = vaccGHIJKLMNx0;
      float16x8_t vaccOPQRSTUV = vaccOPQRSTUVx0;
      vacc01234567 = vaddq_f16(vacc01234567, vacc01234567x1);
      vacc89ABCDEF = vaddq_f16(vacc89ABCDEF, vacc89ABCDEFx1);
      vaccGHIJKLMN = vaddq_f16(vaccGHIJKLMN, vaccGHIJKLMNx1);
      vaccOPQRSTUV = vaddq_f16(vaccOPQRSTUV, vaccOPQRSTUVx1);
      if XNN_LIKELY(nnz != 0) {
        do {
          const intptr_t diff = *dmap++;
          const float16x8_t va01234567 = vld1q_f16(i);
          const float16x8_t va89ABCDEF = vld1q_f16(i + 8);
          const float16x8_t vaGHIJKLMN = vld1q_f16(i + 16);
          const float16x8_t vaOPQRSTUV = vld1q_f16(i + 24);
          i = (const __fp16*restrict) ((uintptr_t) i + (uintptr_t) diff);
          const float16x8_t vb = vld1q_dup_f16(w); w += 1;
          vacc01234567 = vfmaq_f16(vacc01234567, va01234567, vb);
          vacc89ABCDEF = vfmaq_f16(vacc89ABCDEF, va89ABCDEF, vb);
          vaccGHIJKLMN = vfmaq_f16(vaccGHIJKLMN, vaGHIJKLMN, vb);
          vaccOPQRSTUV = vfmaq_f16(vaccOPQRSTUV, vaOPQRSTUV, vb);
        } while (--nnz != 0);
      }
      float16x8_t vout01234567 = vminq_f16(vacc01234567, vmax);
      float16x8_t vout89ABCDEF = vminq_f16(vacc89ABCDEF, vmax);
      float16x8_t voutGHIJKLMN = vminq_f16(vaccGHIJKLMN, vmax);
      float16x8_t voutOPQRSTUV = vminq_f16(vaccOPQRSTUV, vmax);
      vout01234567 = vmaxq_f16(vout01234567, vmin);
      vout89ABCDEF = vmaxq_f16(vout89ABCDEF, vmin);
      voutGHIJKLMN = vmaxq_f16(voutGHIJKLMN, vmin);
      voutOPQRSTUV = vmaxq_f16(voutOPQRSTUV, vmin);
      vst1q_f16(o, vout01234567);
      vst1q_f16(o + 8, vout89ABCDEF);
      vst1q_f16(o + 16, voutGHIJKLMN);
      vst1q_f16(o + 24, voutOPQRSTUV);
      o = (__fp16*restrict) ((uintptr_t) o + output_stride);
    } while (--n != 0);
    o = (__fp16*restrict) ((uintptr_t) o - output_decrement);
    i += 32;
    mc -= 32 * sizeof(__fp16);
  }
  if XNN_UNLIKELY(mc != 0) {
    output_decrement += 16 * sizeof(__fp16);
    if (mc & (16 * sizeof(__fp16))) {
      const __fp16*restrict w = (const __fp16*) weights;
      const int32_t* dmap = widx_dmap;
      const uint32_t* nnzmap = nidx_nnzmap;
      size_t n = nc;
      do {
        uint32_t nnz = *nnzmap++;
        float16x8_t vacc01234567 = vld1q_dup_f16(w); w += 1;
        float16x8_t vacc89ABCDEF = vacc01234567;
        if XNN_LIKELY(nnz != 0) {
          do {
            const intptr_t diff = *dmap++;
            const float16x8_t va01234567 = vld1q_f16(i);
            const float16x8_t va89ABCDEF = vld1q_f16(i + 8);
            i = (const __fp16*restrict) ((uintptr_t) i + (uintptr_t) diff);
            const float16x8_t vb = vld1q_dup_f16(w); w += 1;
            vacc01234567 = vfmaq_f16(vacc01234567, va01234567, vb);
            vacc89ABCDEF = vfmaq_f16(vacc89ABCDEF, va89ABCDEF, vb);
          } while (--nnz != 0);
        }
        float16x8_t vout01234567 = vminq_f16(vacc01234567, vmax);
        float16x8_t vout89ABCDEF = vminq_f16(vacc89ABCDEF, vmax);
        vout01234567 = vmaxq_f16(vout01234567, vmin);
        vout89ABCDEF = vmaxq_f16(vout89ABCDEF, vmin);
        vst1q_f16(o, vout01234567);
        vst1q_f16(o + 8, vout89ABCDEF);
        o = (__fp16*restrict) ((uintptr_t) o + output_stride);
      } while (--n != 0);
      o = (__fp16*restrict) ((uintptr_t) o - output_decrement);
      i += 16;
    }
    output_decrement += 8 * sizeof(__fp16);
    if (mc & (8 * sizeof(__fp16))) {
      const __fp16*restrict w = (const __fp16*) weights;
      const int32_t* dmap = widx_dmap;
      const uint32_t* nnzmap = nidx_nnzmap;
      size_t n = nc;
      do {
        uint32_t nnz = *nnzmap++;
        float16x8_t vacc01234567 = vld1q_dup_f16(w); w += 1;
        if XNN_LIKELY(nnz != 0) {
          do {
            const intptr_t diff = *dmap++;
            const float16x8_t va01234567 = vld1q_f16(i);
            i = (const __fp16*restrict) ((uintptr_t) i + (uintptr_t) diff);
            const float16x8_t vb = vld1q_dup_f16(w); w += 1;
            vacc01234567 = vfmaq_f16(vacc01234567, va01234567, vb);
          } while (--nnz != 0);
        }
        float16x8_t vout01234567 = vminq_f16(vacc01234567, vmax);
        vout01234567 = vmaxq_f16(vout01234567, vmin);
        vst1q_f16(o, vout01234567);
        o = (__fp16*restrict) ((uintptr_t) o + output_stride);
      } while (--n != 0);
      o = (__fp16*restrict) ((uintptr_t) o - output_decrement);
      i += 8;
    }
    output_decrement += 4 * sizeof(__fp16);
    if (mc & (4 * sizeof(__fp16))) {
      const __fp16*restrict w = (const __fp16*) weights;
      const int32_t* dmap = widx_dmap;
      const uint32_t* nnzmap = nidx_nnzmap;
      size_t n = nc;
      do {
        uint32_t nnz = *nnzmap++;
        float16x4_t vacc0123 = vld1_dup_f16(w); w += 1;
        if XNN_LIKELY(nnz != 0) {
          do {
            const intptr_t diff = *dmap++;
            const float16x4_t va0123 = vld1_f16(i);
            i = (const __fp16*restrict) ((uintptr_t) i + (uintptr_t) diff);
            const float16x4_t vb = vld1_dup_f16(w); w += 1;
            vacc0123 = vfma_f16(vacc0123, va0123, vb);
          } while (--nnz != 0);
        }
        float16x4_t vout0123 = vmin_f16(vacc0123, vget_low_f16(vmax));
        vout0123 = vmax_f16(vout0123, vget_low_f16(vmin));
        vst1_f16(o, vout0123);
        o = (__fp16*restrict) ((uintptr_t) o + output_stride);
      } while (--n != 0);
      o = (__fp16*restrict) ((uintptr_t) o - output_decrement);
      i += 4;
    }
    output_decrement += 2 * sizeof(__fp16);
    if (mc & (2 * sizeof(__fp16))) {
      const __fp16*restrict w = (const __fp16*) weights;
      const int32_t* dmap = widx_dmap;
      const uint32_t* nnzmap = nidx_nnzmap;
      size_t n = nc;
      do {
        uint32_t nnz = *nnzmap++;
        float16x4_t vacc01 = vld1_dup_f16(w); w += 1;
        if XNN_LIKELY(nnz != 0) {
          do {
            const intptr_t diff = *dmap++;
            const float16x4_t va01 = vreinterpret_f16_f32(vld1_dup_f32((const void*) i));
            i = (const __fp16*restrict) ((uintptr_t) i + (uintptr_t) diff);
            const float16x4_t vb = vld1_dup_f16(w); w += 1;
            vacc01 = vfma_f16(vacc01, va01, vb);
          } while (--nnz != 0);
        }
        float16x4_t vout01 = vmin_f16(vacc01, vget_low_f16(vmax));
        vout01 = vmax_f16(vout01, vget_low_f16(vmin));
        vst1_lane_f32((void*) o, vreinterpret_f32_f16(vout01), 0);
        o = (__fp16*restrict) ((uintptr_t) o + output_stride);
      } while (--n != 0);
      o = (__fp16*restrict) ((uintptr_t) o - output_decrement);
      i += 2;
    }
    output_decrement += 1 * sizeof(__fp16);
    if (mc & (1 * sizeof(__fp16))) {
      const __fp16*restrict w = (const __fp16*) weights;
      const int32_t* dmap = widx_dmap;
      const uint32_t* nnzmap = nidx_nnzmap;
      size_t n = nc;
      do {
        uint32_t nnz = *nnzmap++;
        float16x4_t vacc0 = vld1_dup_f16(w); w += 1;
        if XNN_LIKELY(nnz != 0) {
          do {
            const intptr_t diff = *dmap++;
            const float16x4_t va0 = vld1_dup_f16(i);
            i = (const __fp16*restrict) ((uintptr_t) i + (uintptr_t) diff);
            const float16x4_t vb = vld1_dup_f16(w); w += 1;
            vacc0 = vfma_f16(vacc0, va0, vb);
          } while (--nnz != 0);
        }
        float16x4_t vout0 = vmin_f16(vacc0, vget_low_f16(vmax));
        vout0 = vmax_f16(vout0, vget_low_f16(vmin));
        vst1_lane_f16(o, vout0, 0);
        o = (__fp16*restrict) ((uintptr_t) o + output_stride);
      } while (--n != 0);
      o = (__fp16*restrict) ((uintptr_t) o - output_decrement);
      i += 1;
    }
  }
}

// Auto-generated file. Do not edit!
//   Template: src/x8-lut/neon-tbx128x4.c.in
//   Generator: tools/xngen
//
// Copyright 2021 Google LLC
//
// This source code is licensed under the BSD-style license found in the
// LICENSE file in the root directory of this source tree.

#include <assert.h>

#include <arm_neon.h>

#include <xnnpack/intrinsics-polyfill.h>
#include <xnnpack/lut.h>
#include <xnnpack/common.h>


void xnn_x8_lut_ukernel__neon_tbx128x4_x16(
    size_t n,
    const uint8_t* x,
    uint8_t* y,
    const uint8_t t[restrict XNN_MIN_ELEMENTS(256)])
{
  assert(n != 0);
  assert(x != NULL);
  assert(y != NULL);

  const uint8x16x4_t vtable0123 = vld1q_u8_x4(t);
  const uint8x16x4_t vtable4567 = vld1q_u8_x4(t + 64);
  const uint8x16x4_t vtable89AB = vld1q_u8_x4(t + 128);
  const uint8x16x4_t vtableCDEF = vld1q_u8_x4(t + 192);
  const uint8x16_t voffset = vmovq_n_u8(64);
  for (; n >= 16 * sizeof(uint8_t); n -= 16 * sizeof(uint8_t)) {
    uint8x16_t vx = vld1q_u8(x); x += 16;

    uint8x16_t vy = vqtbl4q_u8(vtable0123, vx);

    vx = vsubq_u8(vx, voffset);
    vy = vqtbx4q_u8(vy, vtable4567, vx);

    vx = vsubq_u8(vx, voffset);
    vy = vqtbx4q_u8(vy, vtable89AB, vx);

    vx = vsubq_u8(vx, voffset);
    vy = vqtbx4q_u8(vy, vtableCDEF, vx);

    vst1q_u8(y, vy); y += 16;
  }
  if XNN_UNLIKELY(n != 0) {
    uint8x16_t vx = vld1q_u8(x);

    uint8x16_t vy = vqtbl4q_u8(vtable0123, vx);

    vx = vsubq_u8(vx, voffset);
    vy = vqtbx4q_u8(vy, vtable4567, vx);

    vx = vsubq_u8(vx, voffset);
    vy = vqtbx4q_u8(vy, vtable89AB, vx);

    vx = vsubq_u8(vx, voffset);
    vy = vqtbx4q_u8(vy, vtableCDEF, vx);

    uint8x8_t vy_lo = vget_low_u8(vy);
    if (n & (8 * sizeof(uint8_t))) {
      vst1_u8(y, vy_lo); y += 8;
      vy_lo = vget_high_u8(vy);
    }
    if (n & (4 * sizeof(uint8_t))) {
      vst1_lane_u32((void*) y, vreinterpret_u32_u8(vy_lo), 0); y += 4;
      vy_lo = vext_u8(vy_lo, vy_lo, 4);
    }
    if (n & (2 * sizeof(uint8_t))) {
      vst1_lane_u16((void*) y, vreinterpret_u16_u8(vy_lo), 0); y += 2;
      vy_lo = vext_u8(vy_lo, vy_lo, 2);
    }
    if (n & (1 * sizeof(uint8_t))) {
      vst1_lane_u8(y, vy_lo, 0);
    }
  }
}

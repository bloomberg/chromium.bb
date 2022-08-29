// Auto-generated file. Do not edit!
//   Template: src/f32-vlrelu/wasm.c.in
//   Generator: tools/xngen
//
// Copyright 2020 Google LLC
//
// This source code is licensed under the BSD-style license found in the
// LICENSE file in the root directory of this source tree.

#include <assert.h>

#include <xnnpack/common.h>
#include <xnnpack/vunary.h>


void xnn_f32_vlrelu_ukernel__wasm_x2(
    size_t n,
    const float* x,
    float* y,
    const union xnn_f32_lrelu_params params[restrict XNN_MIN_ELEMENTS(1)])
{
  assert(n != 0);
  assert(n % sizeof(float) == 0);

  const float vslope = params->scalar.slope;
  const float vzero = 0.0f;

  for (; n >= 2 * sizeof(float); n -= 2 * sizeof(float)) {
    const float vx0 = x[0];
    const float vx1 = x[1];
    x += 2;

    const float vnegx0 = __builtin_wasm_min_f32(vx0, vzero);
    const float vnegx1 = __builtin_wasm_min_f32(vx1, vzero);

    float vacc0 = vnegx0 * vslope;
    const float vposx0 = __builtin_wasm_max_f32(vx0, vzero);
    float vacc1 = vnegx1 * vslope;
    const float vposx1 = __builtin_wasm_max_f32(vx1, vzero);

    vacc0 += vposx0;
    vacc1 += vposx1;

    y[0] = vacc0;
    y[1] = vacc1;
    y += 2;
  }
  if XNN_UNLIKELY(n != 0) {
    const float vx = *x;
    const float vnegx = __builtin_wasm_min_f32(vx, vzero);
    float vacc = vnegx * vslope;
    const float vposx = __builtin_wasm_max_f32(vx, vzero);
    vacc += vposx;
    *y = vacc;
  }
}

// Auto-generated file. Do not edit!
//   Template: src/f32-vlrelu/wasmsimd-minmax.c.in
//   Generator: tools/xngen
//
// Copyright 2020 Google LLC
//
// This source code is licensed under the BSD-style license found in the
// LICENSE file in the root directory of this source tree.

#include <assert.h>

#include <wasm_simd128.h>

#include <xnnpack/common.h>
#include <xnnpack/vunary.h>


void xnn_f32_vlrelu_ukernel__wasmsimd_minmax_x4(
    size_t n,
    const float* x,
    float* y,
    const union xnn_f32_lrelu_params params[restrict XNN_MIN_ELEMENTS(1)]) XNN_OOB_READS
{
  assert(n != 0);
  assert(n % sizeof(float) == 0);

  const v128_t vslope = wasm_v128_load64_splat(params->wasmsimd.slope);
  const v128_t vzero = wasm_i32x4_const_splat(0);
  for (; n >= 4 * sizeof(float); n -= 4 * sizeof(float)) {
    v128_t vx = wasm_v128_load(x);
    x += 4;
    v128_t vacc = wasm_i32x4_max(vx, vzero);
    vx = wasm_i32x4_min(vx, vzero);
    vacc = wasm_f32x4_add(vacc, wasm_f32x4_mul(vx, vslope));
    wasm_v128_store(y, vacc);
    y += 4;
  }
  if XNN_UNLIKELY(n != 0) {
    v128_t vx = wasm_v128_load(x);
    v128_t vacc = wasm_i32x4_max(vx, vzero);
    vx = wasm_i32x4_min(vx, vzero);
    vacc = wasm_f32x4_add(vacc, wasm_f32x4_mul(vx, vslope));

    if (n & (2 * sizeof(float))) {
      *((double*) y) = wasm_f64x2_extract_lane(vacc, 0);
      vacc = wasm_v32x4_shuffle(vacc, vacc, 2, 3, 2, 3);
      y += 2;
    }
    if (n & (1 * sizeof(float))) {
      *y = wasm_f32x4_extract_lane(vacc, 0);
    }
  }
}

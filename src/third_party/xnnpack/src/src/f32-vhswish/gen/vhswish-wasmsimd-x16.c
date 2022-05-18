// Auto-generated file. Do not edit!
//   Template: src/f32-vhswish/wasmsimd.c.in
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


void xnn_f32_vhswish_ukernel__wasmsimd_x16(
    size_t n,
    const float* x,
    float* y,
    const union xnn_f32_hswish_params params[restrict XNN_MIN_ELEMENTS(1)]) XNN_OOB_READS
{
  assert(n != 0);
  assert(n % sizeof(float) == 0);

  const v128_t vsixth = wasm_v128_load64_splat(params->wasmsimd.sixth);
  const v128_t vthree = wasm_v128_load64_splat(params->wasmsimd.three);
  const v128_t vsix = wasm_v128_load64_splat(params->wasmsimd.six);
  const v128_t vzero = wasm_i32x4_const_splat(0);

  for (; n >= 16 * sizeof(float); n -= 16 * sizeof(float)) {
    v128_t vx0123 = wasm_v128_load(x);
    v128_t vx4567 = wasm_v128_load(x + 4);
    v128_t vx89AB = wasm_v128_load(x + 8);
    v128_t vxCDEF = wasm_v128_load(x + 12);
    x += 16;

    v128_t vacc0123 = wasm_f32x4_add(vx0123, vthree);
    vx0123 = wasm_f32x4_mul(vx0123, vsixth);
    v128_t vacc4567 = wasm_f32x4_add(vx4567, vthree);
    vx4567 = wasm_f32x4_mul(vx4567, vsixth);
    v128_t vacc89AB = wasm_f32x4_add(vx89AB, vthree);
    vx89AB = wasm_f32x4_mul(vx89AB, vsixth);
    v128_t vaccCDEF = wasm_f32x4_add(vxCDEF, vthree);
    vxCDEF = wasm_f32x4_mul(vxCDEF, vsixth);

    vacc0123 = wasm_i32x4_max(vacc0123, vzero);
    vacc4567 = wasm_i32x4_max(vacc4567, vzero);
    vacc89AB = wasm_i32x4_max(vacc89AB, vzero);
    vaccCDEF = wasm_i32x4_max(vaccCDEF, vzero);

    vacc0123 = wasm_i32x4_min(vacc0123, vsix);
    vacc4567 = wasm_i32x4_min(vacc4567, vsix);
    vacc89AB = wasm_i32x4_min(vacc89AB, vsix);
    vaccCDEF = wasm_i32x4_min(vaccCDEF, vsix);

    vacc0123 = wasm_f32x4_mul(vacc0123, vx0123);
    vacc4567 = wasm_f32x4_mul(vacc4567, vx4567);
    vacc89AB = wasm_f32x4_mul(vacc89AB, vx89AB);
    vaccCDEF = wasm_f32x4_mul(vaccCDEF, vxCDEF);

    wasm_v128_store(y, vacc0123);
    wasm_v128_store(y + 4, vacc4567);
    wasm_v128_store(y + 8, vacc89AB);
    wasm_v128_store(y + 12, vaccCDEF);
    y += 16;
  }
  for (; n >= 4 * sizeof(float); n -= 4 * sizeof(float)) {
    v128_t vx = wasm_v128_load(x);
    x += 4;

    v128_t vacc = wasm_f32x4_add(vx, vthree);
    vx = wasm_f32x4_mul(vx, vsixth);
    vacc = wasm_i32x4_max(vacc, vzero);
    vacc = wasm_i32x4_min(vacc, vsix);
    vacc = wasm_f32x4_mul(vacc, vx);

    wasm_v128_store(y, vacc);
    y += 4;
  }
  if XNN_UNLIKELY(n != 0) {
    v128_t vx = wasm_v128_load(x);

    v128_t vacc = wasm_f32x4_add(vx, vthree);
    vx = wasm_f32x4_mul(vx, vsixth);
    vacc = wasm_i32x4_max(vacc, vzero);
    vacc = wasm_i32x4_min(vacc, vsix);
    vacc = wasm_f32x4_mul(vacc, vx);

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

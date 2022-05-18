// Auto-generated file. Do not edit!
//   Template: src/qs8-vmulc/wasmsimd-mul32-ld64.c.in
//   Generator: tools/xngen
//
// Copyright 2021 Google LLC
//
// This source code is licensed under the BSD-style license found in the
// LICENSE file in the root directory of this source tree.

#include <assert.h>

#include <wasm_simd128.h>

#include <xnnpack/vmul.h>


void xnn_qs8_vmulc_minmax_fp32_ukernel__wasmsimd_mul32_ld64_x8(
    size_t n,
    const int8_t* input_a,
    const int8_t* input_b,
    int8_t* output,
    const union xnn_qs8_mul_minmax_params params[restrict XNN_MIN_ELEMENTS(1)]) XNN_OOB_READS

{
  const v128_t va_zero_point = wasm_v128_load64_splat(params->fp32_wasmsimd.a_zero_point);
  const v128_t vscale = wasm_v128_load64_splat(params->fp32_wasmsimd.scale);
  const v128_t vmagic_bias = wasm_v128_load64_splat(params->fp32_wasmsimd.magic_bias);
  const v128_t vmagic_min = wasm_v128_load64_splat(params->fp32_wasmsimd.magic_min);
  const v128_t vmagic_bias_less_output_zero_point = wasm_v128_load64_splat(params->fp32_wasmsimd.magic_bias_less_output_zero_point);
  const v128_t voutput_max = wasm_v128_load64_splat(params->fp32_wasmsimd.output_max);

  const v128_t vxb = wasm_i16x8_sub(
    wasm_i16x8_splat((int16_t) *input_b), wasm_v128_load64_splat(params->fp32_wasmsimd.b_zero_point));
  const v128_t vxblo = wasm_i32x4_extend_low_i16x8(vxb);
  const v128_t vxbhi = wasm_i32x4_extend_high_i16x8(vxb);
  for (; n >= 8 * sizeof(int8_t); n -= 8 * sizeof(int8_t)) {
    const v128_t va01234567 = wasm_i16x8_load8x8(input_a);
    input_a += 8;

    const v128_t vxa01234567 = wasm_i16x8_sub(va01234567, va_zero_point);

    v128_t vacc0123 = wasm_i32x4_mul(wasm_i32x4_extend_low_i16x8(vxa01234567), vxblo);
    v128_t vacc4567 = wasm_i32x4_mul(wasm_i32x4_extend_high_i16x8(vxa01234567), vxbhi);

    vacc0123 = wasm_f32x4_convert_i32x4(vacc0123);
    vacc4567 = wasm_f32x4_convert_i32x4(vacc4567);

    vacc0123 = wasm_f32x4_mul(vacc0123, vscale);
    vacc4567 = wasm_f32x4_mul(vacc4567, vscale);

    vacc0123 = wasm_f32x4_add(vacc0123, vmagic_bias);
    vacc4567 = wasm_f32x4_add(vacc4567, vmagic_bias);

    vacc0123 = wasm_i32x4_max(vacc0123, vmagic_min);
    vacc4567 = wasm_i32x4_max(vacc4567, vmagic_min);

    vacc0123 = wasm_i32x4_sub(vacc0123, vmagic_bias_less_output_zero_point);
    vacc4567 = wasm_i32x4_sub(vacc4567, vmagic_bias_less_output_zero_point);

    v128_t vout01234567 = wasm_i16x8_narrow_i32x4(vacc0123, vacc4567);

    v128_t vout0123456701234567 = wasm_i8x16_narrow_i16x8(vout01234567, vout01234567);

    vout0123456701234567 = wasm_i8x16_min(vout0123456701234567, voutput_max);

    *((double*) output) = wasm_f64x2_extract_lane(vout0123456701234567, 0);
    output += 8;
  }
  if XNN_UNLIKELY(n != 0) {
    {
      const v128_t va01234567 = wasm_i16x8_load8x8(input_a);

      const v128_t vxa01234567 = wasm_i16x8_sub(va01234567, va_zero_point);

      v128_t vacc0123 = wasm_i32x4_mul(wasm_i32x4_extend_low_i16x8(vxa01234567), vxblo);
      v128_t vacc4567 = wasm_i32x4_mul(wasm_i32x4_extend_high_i16x8(vxa01234567), vxbhi);

      vacc0123 = wasm_f32x4_convert_i32x4(vacc0123);
      vacc4567 = wasm_f32x4_convert_i32x4(vacc4567);

      vacc0123 = wasm_f32x4_mul(vacc0123, vscale);
      vacc4567 = wasm_f32x4_mul(vacc4567, vscale);

      vacc0123 = wasm_f32x4_add(vacc0123, vmagic_bias);
      vacc4567 = wasm_f32x4_add(vacc4567, vmagic_bias);

      vacc0123 = wasm_i32x4_max(vacc0123, vmagic_min);
      vacc4567 = wasm_i32x4_max(vacc4567, vmagic_min);

      vacc0123 = wasm_i32x4_sub(vacc0123, vmagic_bias_less_output_zero_point);
      vacc4567 = wasm_i32x4_sub(vacc4567, vmagic_bias_less_output_zero_point);

      v128_t vout01234567 = wasm_i16x8_narrow_i32x4(vacc0123, vacc4567);
      v128_t vout0123456701234567 = wasm_i8x16_narrow_i16x8(vout01234567, vout01234567);
      vout0123456701234567 = wasm_i8x16_min(vout0123456701234567, voutput_max);

      if (n & (4 * sizeof(int8_t))) {
        *((float*) output) = wasm_f32x4_extract_lane(vout0123456701234567, 0);
        vout0123456701234567 = wasm_u64x2_shr(vout0123456701234567, 32);
        output += 4;
      }
      uint32_t vout0123 = wasm_i32x4_extract_lane(vout0123456701234567, 0);
      if (n & (2 * sizeof(int8_t))) {
        *((uint16_t*) output) = (uint16_t) vout0123;
        vout0123 >>= 16;
        output += 2;
      }
      if (n & (1 * sizeof(int8_t))) {
        *output = (int8_t) vout0123;
      }
    }
  }
}

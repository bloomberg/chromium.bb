// Auto-generated file. Do not edit!
//   Template: src/f32-vsigmoid/wasmsimd-rr2-lut64-p2-div.c.in
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


extern XNN_INTERNAL const float xnn_table_exp2minus_k_over_64[64];

void xnn_f32_vsigmoid_ukernel__wasmsimd_rr2_lut64_p2_div_x24(
    size_t n,
    const float* x,
    float* y,
    const union xnn_f32_sigmoid_params params[restrict XNN_MIN_ELEMENTS(1)]) XNN_OOB_READS
{
  assert(n % sizeof(float) == 0);

  const v128_t vmagic_bias = wasm_v128_load64_splat(params->wasmsimd_rr2_lut64_p2.magic_bias);
  const v128_t vminus_log2e = wasm_v128_load64_splat(params->wasmsimd_rr2_lut64_p2.minus_log2e);
  const v128_t vindex_mask = wasm_v128_load64_splat(params->wasmsimd_rr2_lut64_p2.index_mask);
  const v128_t vln2_hi = wasm_v128_load64_splat(params->wasmsimd_rr2_lut64_p2.ln2_hi);
  const v128_t vln2_lo = wasm_v128_load64_splat(params->wasmsimd_rr2_lut64_p2.ln2_lo);
  const v128_t vc2 = wasm_v128_load64_splat(params->wasmsimd_rr2_lut64_p2.c2);
  const v128_t vone = wasm_v128_load64_splat(params->wasmsimd_rr2_lut64_p2.one);
  const v128_t vdenorm_cutoff = wasm_v128_load64_splat(params->wasmsimd_rr2_lut64_p2.denorm_cutoff);

  for (; n >= 24 * sizeof(float); n -= 24 * sizeof(float)) {
    const v128_t vx0123 = wasm_v128_load(x);
    const v128_t vx4567 = wasm_v128_load(x + 4);
    const v128_t vx89AB = wasm_v128_load(x + 8);
    const v128_t vxCDEF = wasm_v128_load(x + 12);
    const v128_t vxGHIJ = wasm_v128_load(x + 16);
    const v128_t vxKLMN = wasm_v128_load(x + 20);
    x += 24;

    const v128_t vz0123 = wasm_f32x4_abs(vx0123);
    const v128_t vz4567 = wasm_f32x4_abs(vx4567);
    const v128_t vz89AB = wasm_f32x4_abs(vx89AB);
    const v128_t vzCDEF = wasm_f32x4_abs(vxCDEF);
    const v128_t vzGHIJ = wasm_f32x4_abs(vxGHIJ);
    const v128_t vzKLMN = wasm_f32x4_abs(vxKLMN);

    v128_t vn0123 = wasm_f32x4_add(vmagic_bias, wasm_f32x4_mul(vz0123, vminus_log2e));
    v128_t vn4567 = wasm_f32x4_add(vmagic_bias, wasm_f32x4_mul(vz4567, vminus_log2e));
    v128_t vn89AB = wasm_f32x4_add(vmagic_bias, wasm_f32x4_mul(vz89AB, vminus_log2e));
    v128_t vnCDEF = wasm_f32x4_add(vmagic_bias, wasm_f32x4_mul(vzCDEF, vminus_log2e));
    v128_t vnGHIJ = wasm_f32x4_add(vmagic_bias, wasm_f32x4_mul(vzGHIJ, vminus_log2e));
    v128_t vnKLMN = wasm_f32x4_add(vmagic_bias, wasm_f32x4_mul(vzKLMN, vminus_log2e));

    const v128_t ve0123 = wasm_i32x4_shl(vn0123, 17);
    const v128_t ve4567 = wasm_i32x4_shl(vn4567, 17);
    const v128_t ve89AB = wasm_i32x4_shl(vn89AB, 17);
    const v128_t veCDEF = wasm_i32x4_shl(vnCDEF, 17);
    const v128_t veGHIJ = wasm_i32x4_shl(vnGHIJ, 17);
    const v128_t veKLMN = wasm_i32x4_shl(vnKLMN, 17);

    const v128_t vidx0123 = wasm_i32x4_shl(wasm_v128_and(vn0123, vindex_mask), 2);
    const v128_t vidx4567 = wasm_i32x4_shl(wasm_v128_and(vn4567, vindex_mask), 2);
    const v128_t vidx89AB = wasm_i32x4_shl(wasm_v128_and(vn89AB, vindex_mask), 2);
    const v128_t vidxCDEF = wasm_i32x4_shl(wasm_v128_and(vnCDEF, vindex_mask), 2);
    const v128_t vidxGHIJ = wasm_i32x4_shl(wasm_v128_and(vnGHIJ, vindex_mask), 2);
    const v128_t vidxKLMN = wasm_i32x4_shl(wasm_v128_and(vnKLMN, vindex_mask), 2);

    const uint64_t vidx01 = wasm_i64x2_extract_lane(vidx0123, 0);
    const uint64_t vidx23 = wasm_i64x2_extract_lane(vidx0123, 1);
    const float vl0   = *((const float*) ((uintptr_t) xnn_table_exp2minus_k_over_64 + (uint32_t) vidx01));
    const float vl1 = *((const float*) ((uintptr_t) xnn_table_exp2minus_k_over_64 + (uint32_t) (vidx01 >> 32)));
    const float vl2 = *((const float*) ((uintptr_t) xnn_table_exp2minus_k_over_64 + (uint32_t) vidx23));
    const float vl3 = *((const float*) ((uintptr_t) xnn_table_exp2minus_k_over_64 + (uint32_t) (vidx23 >> 32)));
    const v128_t vl0123 = wasm_f32x4_make(vl0, vl1, vl2, vl3);
    const uint64_t vidx45 = wasm_i64x2_extract_lane(vidx4567, 0);
    const uint64_t vidx67 = wasm_i64x2_extract_lane(vidx4567, 1);
    const float vl4   = *((const float*) ((uintptr_t) xnn_table_exp2minus_k_over_64 + (uint32_t) vidx45));
    const float vl5 = *((const float*) ((uintptr_t) xnn_table_exp2minus_k_over_64 + (uint32_t) (vidx45 >> 32)));
    const float vl6 = *((const float*) ((uintptr_t) xnn_table_exp2minus_k_over_64 + (uint32_t) vidx67));
    const float vl7 = *((const float*) ((uintptr_t) xnn_table_exp2minus_k_over_64 + (uint32_t) (vidx67 >> 32)));
    const v128_t vl4567 = wasm_f32x4_make(vl4, vl5, vl6, vl7);
    const uint64_t vidx89 = wasm_i64x2_extract_lane(vidx89AB, 0);
    const uint64_t vidxAB = wasm_i64x2_extract_lane(vidx89AB, 1);
    const float vl8   = *((const float*) ((uintptr_t) xnn_table_exp2minus_k_over_64 + (uint32_t) vidx89));
    const float vl9 = *((const float*) ((uintptr_t) xnn_table_exp2minus_k_over_64 + (uint32_t) (vidx89 >> 32)));
    const float vlA = *((const float*) ((uintptr_t) xnn_table_exp2minus_k_over_64 + (uint32_t) vidxAB));
    const float vlB = *((const float*) ((uintptr_t) xnn_table_exp2minus_k_over_64 + (uint32_t) (vidxAB >> 32)));
    const v128_t vl89AB = wasm_f32x4_make(vl8, vl9, vlA, vlB);
    const uint64_t vidxCD = wasm_i64x2_extract_lane(vidxCDEF, 0);
    const uint64_t vidxEF = wasm_i64x2_extract_lane(vidxCDEF, 1);
    const float vlC   = *((const float*) ((uintptr_t) xnn_table_exp2minus_k_over_64 + (uint32_t) vidxCD));
    const float vlD = *((const float*) ((uintptr_t) xnn_table_exp2minus_k_over_64 + (uint32_t) (vidxCD >> 32)));
    const float vlE = *((const float*) ((uintptr_t) xnn_table_exp2minus_k_over_64 + (uint32_t) vidxEF));
    const float vlF = *((const float*) ((uintptr_t) xnn_table_exp2minus_k_over_64 + (uint32_t) (vidxEF >> 32)));
    const v128_t vlCDEF = wasm_f32x4_make(vlC, vlD, vlE, vlF);
    const uint64_t vidxGH = wasm_i64x2_extract_lane(vidxGHIJ, 0);
    const uint64_t vidxIJ = wasm_i64x2_extract_lane(vidxGHIJ, 1);
    const float vlG   = *((const float*) ((uintptr_t) xnn_table_exp2minus_k_over_64 + (uint32_t) vidxGH));
    const float vlH = *((const float*) ((uintptr_t) xnn_table_exp2minus_k_over_64 + (uint32_t) (vidxGH >> 32)));
    const float vlI = *((const float*) ((uintptr_t) xnn_table_exp2minus_k_over_64 + (uint32_t) vidxIJ));
    const float vlJ = *((const float*) ((uintptr_t) xnn_table_exp2minus_k_over_64 + (uint32_t) (vidxIJ >> 32)));
    const v128_t vlGHIJ = wasm_f32x4_make(vlG, vlH, vlI, vlJ);
    const uint64_t vidxKL = wasm_i64x2_extract_lane(vidxKLMN, 0);
    const uint64_t vidxMN = wasm_i64x2_extract_lane(vidxKLMN, 1);
    const float vlK   = *((const float*) ((uintptr_t) xnn_table_exp2minus_k_over_64 + (uint32_t) vidxKL));
    const float vlL = *((const float*) ((uintptr_t) xnn_table_exp2minus_k_over_64 + (uint32_t) (vidxKL >> 32)));
    const float vlM = *((const float*) ((uintptr_t) xnn_table_exp2minus_k_over_64 + (uint32_t) vidxMN));
    const float vlN = *((const float*) ((uintptr_t) xnn_table_exp2minus_k_over_64 + (uint32_t) (vidxMN >> 32)));
    const v128_t vlKLMN = wasm_f32x4_make(vlK, vlL, vlM, vlN);

    const v128_t vs0123 = wasm_i32x4_add(vl0123, ve0123);
    const v128_t vs4567 = wasm_i32x4_add(vl4567, ve4567);
    const v128_t vs89AB = wasm_i32x4_add(vl89AB, ve89AB);
    const v128_t vsCDEF = wasm_i32x4_add(vlCDEF, veCDEF);
    const v128_t vsGHIJ = wasm_i32x4_add(vlGHIJ, veGHIJ);
    const v128_t vsKLMN = wasm_i32x4_add(vlKLMN, veKLMN);

    vn0123 = wasm_f32x4_sub(vn0123, vmagic_bias);
    vn4567 = wasm_f32x4_sub(vn4567, vmagic_bias);
    vn89AB = wasm_f32x4_sub(vn89AB, vmagic_bias);
    vnCDEF = wasm_f32x4_sub(vnCDEF, vmagic_bias);
    vnGHIJ = wasm_f32x4_sub(vnGHIJ, vmagic_bias);
    vnKLMN = wasm_f32x4_sub(vnKLMN, vmagic_bias);

    v128_t vt0123 = wasm_f32x4_add(vz0123, wasm_f32x4_mul(vn0123, vln2_hi));
    v128_t vt4567 = wasm_f32x4_add(vz4567, wasm_f32x4_mul(vn4567, vln2_hi));
    v128_t vt89AB = wasm_f32x4_add(vz89AB, wasm_f32x4_mul(vn89AB, vln2_hi));
    v128_t vtCDEF = wasm_f32x4_add(vzCDEF, wasm_f32x4_mul(vnCDEF, vln2_hi));
    v128_t vtGHIJ = wasm_f32x4_add(vzGHIJ, wasm_f32x4_mul(vnGHIJ, vln2_hi));
    v128_t vtKLMN = wasm_f32x4_add(vzKLMN, wasm_f32x4_mul(vnKLMN, vln2_hi));

    vt0123 = wasm_f32x4_add(vt0123, wasm_f32x4_mul(vn0123, vln2_lo));
    vt4567 = wasm_f32x4_add(vt4567, wasm_f32x4_mul(vn4567, vln2_lo));
    vt89AB = wasm_f32x4_add(vt89AB, wasm_f32x4_mul(vn89AB, vln2_lo));
    vtCDEF = wasm_f32x4_add(vtCDEF, wasm_f32x4_mul(vnCDEF, vln2_lo));
    vtGHIJ = wasm_f32x4_add(vtGHIJ, wasm_f32x4_mul(vnGHIJ, vln2_lo));
    vtKLMN = wasm_f32x4_add(vtKLMN, wasm_f32x4_mul(vnKLMN, vln2_lo));

    v128_t vp0123 = wasm_f32x4_mul(vt0123, vc2);
    v128_t vp4567 = wasm_f32x4_mul(vt4567, vc2);
    v128_t vp89AB = wasm_f32x4_mul(vt89AB, vc2);
    v128_t vpCDEF = wasm_f32x4_mul(vtCDEF, vc2);
    v128_t vpGHIJ = wasm_f32x4_mul(vtGHIJ, vc2);
    v128_t vpKLMN = wasm_f32x4_mul(vtKLMN, vc2);

    vp0123 = wasm_f32x4_sub(vt0123, wasm_f32x4_mul(vp0123, vt0123));
    vp4567 = wasm_f32x4_sub(vt4567, wasm_f32x4_mul(vp4567, vt4567));
    vp89AB = wasm_f32x4_sub(vt89AB, wasm_f32x4_mul(vp89AB, vt89AB));
    vpCDEF = wasm_f32x4_sub(vtCDEF, wasm_f32x4_mul(vpCDEF, vtCDEF));
    vpGHIJ = wasm_f32x4_sub(vtGHIJ, wasm_f32x4_mul(vpGHIJ, vtGHIJ));
    vpKLMN = wasm_f32x4_sub(vtKLMN, wasm_f32x4_mul(vpKLMN, vtKLMN));

    const v128_t vy0123 = wasm_f32x4_sub(vs0123, wasm_f32x4_mul(vs0123, vp0123));
    const v128_t vy4567 = wasm_f32x4_sub(vs4567, wasm_f32x4_mul(vs4567, vp4567));
    const v128_t vy89AB = wasm_f32x4_sub(vs89AB, wasm_f32x4_mul(vs89AB, vp89AB));
    const v128_t vyCDEF = wasm_f32x4_sub(vsCDEF, wasm_f32x4_mul(vsCDEF, vpCDEF));
    const v128_t vyGHIJ = wasm_f32x4_sub(vsGHIJ, wasm_f32x4_mul(vsGHIJ, vpGHIJ));
    const v128_t vyKLMN = wasm_f32x4_sub(vsKLMN, wasm_f32x4_mul(vsKLMN, vpKLMN));

    const v128_t vd0123 = wasm_f32x4_add(vy0123, vone);
    const v128_t vd4567 = wasm_f32x4_add(vy4567, vone);
    const v128_t vd89AB = wasm_f32x4_add(vy89AB, vone);
    const v128_t vdCDEF = wasm_f32x4_add(vyCDEF, vone);
    const v128_t vdGHIJ = wasm_f32x4_add(vyGHIJ, vone);
    const v128_t vdKLMN = wasm_f32x4_add(vyKLMN, vone);

    v128_t vf0123 = wasm_f32x4_div(vy0123, vd0123);
    v128_t vf4567 = wasm_f32x4_div(vy4567, vd4567);
    v128_t vf89AB = wasm_f32x4_div(vy89AB, vd89AB);
    v128_t vfCDEF = wasm_f32x4_div(vyCDEF, vdCDEF);
    v128_t vfGHIJ = wasm_f32x4_div(vyGHIJ, vdGHIJ);
    v128_t vfKLMN = wasm_f32x4_div(vyKLMN, vdKLMN);

    vf0123 = wasm_v128_andnot(vf0123, wasm_f32x4_gt(vz0123, vdenorm_cutoff));
    vf4567 = wasm_v128_andnot(vf4567, wasm_f32x4_gt(vz4567, vdenorm_cutoff));
    vf89AB = wasm_v128_andnot(vf89AB, wasm_f32x4_gt(vz89AB, vdenorm_cutoff));
    vfCDEF = wasm_v128_andnot(vfCDEF, wasm_f32x4_gt(vzCDEF, vdenorm_cutoff));
    vfGHIJ = wasm_v128_andnot(vfGHIJ, wasm_f32x4_gt(vzGHIJ, vdenorm_cutoff));
    vfKLMN = wasm_v128_andnot(vfKLMN, wasm_f32x4_gt(vzKLMN, vdenorm_cutoff));

    vf0123 = wasm_v128_bitselect(vf0123, wasm_f32x4_sub(vone, vf0123), wasm_i32x4_shr(vx0123, 31));
    vf4567 = wasm_v128_bitselect(vf4567, wasm_f32x4_sub(vone, vf4567), wasm_i32x4_shr(vx4567, 31));
    vf89AB = wasm_v128_bitselect(vf89AB, wasm_f32x4_sub(vone, vf89AB), wasm_i32x4_shr(vx89AB, 31));
    vfCDEF = wasm_v128_bitselect(vfCDEF, wasm_f32x4_sub(vone, vfCDEF), wasm_i32x4_shr(vxCDEF, 31));
    vfGHIJ = wasm_v128_bitselect(vfGHIJ, wasm_f32x4_sub(vone, vfGHIJ), wasm_i32x4_shr(vxGHIJ, 31));
    vfKLMN = wasm_v128_bitselect(vfKLMN, wasm_f32x4_sub(vone, vfKLMN), wasm_i32x4_shr(vxKLMN, 31));

    wasm_v128_store(y, vf0123);
    wasm_v128_store(y + 4, vf4567);
    wasm_v128_store(y + 8, vf89AB);
    wasm_v128_store(y + 12, vfCDEF);
    wasm_v128_store(y + 16, vfGHIJ);
    wasm_v128_store(y + 20, vfKLMN);
    y += 24;
  }
  for (; n >= 4 * sizeof(float); n -= 4 * sizeof(float)) {
    const v128_t vx = wasm_v128_load(x);
    x += 4;

    const v128_t vz = wasm_f32x4_abs(vx);

    v128_t vn = wasm_f32x4_add(vmagic_bias, wasm_f32x4_mul(vz, vminus_log2e));
    const v128_t ve = wasm_i32x4_shl(vn, 17);

    const v128_t vidx = wasm_i32x4_shl(wasm_v128_and(vn, vindex_mask), 2);
    const uint64_t vidx_lo = wasm_i64x2_extract_lane(vidx, 0);
    const uint64_t vidx_hi = wasm_i64x2_extract_lane(vidx, 1);
    const float vl0 = *((const float*) ((uintptr_t) xnn_table_exp2minus_k_over_64 + (uint32_t) vidx_lo));
    const float vl1 = *((const float*) ((uintptr_t) xnn_table_exp2minus_k_over_64 + (uint32_t) (vidx_lo >> 32)));
    const float vl2 = *((const float*) ((uintptr_t) xnn_table_exp2minus_k_over_64 + (uint32_t) vidx_hi));
    const float vl3 = *((const float*) ((uintptr_t) xnn_table_exp2minus_k_over_64 + (uint32_t) (vidx_hi >> 32)));
    const v128_t vl = wasm_f32x4_make(vl0, vl1, vl2, vl3);

    const v128_t vs = wasm_i32x4_add(vl, ve);
    vn = wasm_f32x4_sub(vn, vmagic_bias);

    v128_t vt = wasm_f32x4_add(vz, wasm_f32x4_mul(vn, vln2_hi));
    vt = wasm_f32x4_add(vt, wasm_f32x4_mul(vn, vln2_lo));

    v128_t vp = wasm_f32x4_mul(vt, vc2);
    vp = wasm_f32x4_sub(vt, wasm_f32x4_mul(vp, vt));

    const v128_t vy = wasm_f32x4_sub(vs, wasm_f32x4_mul(vs, vp));
    const v128_t vd = wasm_f32x4_add(vy, vone);

    v128_t vf = wasm_f32x4_div(vy, vd);
    vf = wasm_v128_andnot(vf, wasm_f32x4_gt(vz, vdenorm_cutoff));
    vf = wasm_v128_bitselect(vf, wasm_f32x4_sub(vone, vf), wasm_i32x4_shr(vx, 31));

    wasm_v128_store(y, vf);
    y += 4;
  }
  if XNN_UNLIKELY(n != 0) {
    const v128_t vx = wasm_v128_load(x);

    const v128_t vz = wasm_f32x4_abs(vx);

    v128_t vn = wasm_f32x4_add(vmagic_bias, wasm_f32x4_mul(vz, vminus_log2e));
    const v128_t ve = wasm_i32x4_shl(vn, 17);

    const v128_t vidx = wasm_i32x4_shl(wasm_v128_and(vn, vindex_mask), 2);
    const uint64_t vidx_lo = wasm_i64x2_extract_lane(vidx, 0);
    const uint64_t vidx_hi = wasm_i64x2_extract_lane(vidx, 1);
    const float vl0 = *((const float*) ((uintptr_t) xnn_table_exp2minus_k_over_64 + (uint32_t) vidx_lo));
    const float vl1 = *((const float*) ((uintptr_t) xnn_table_exp2minus_k_over_64 + (uint32_t) (vidx_lo >> 32)));
    const float vl2 = *((const float*) ((uintptr_t) xnn_table_exp2minus_k_over_64 + (uint32_t) vidx_hi));
    const float vl3 = *((const float*) ((uintptr_t) xnn_table_exp2minus_k_over_64 + (uint32_t) (vidx_hi >> 32)));
    const v128_t vl = wasm_f32x4_make(vl0, vl1, vl2, vl3);

    const v128_t vs = wasm_i32x4_add(vl, ve);
    vn = wasm_f32x4_sub(vn, vmagic_bias);

    v128_t vt = wasm_f32x4_add(vz, wasm_f32x4_mul(vn, vln2_hi));
    vt = wasm_f32x4_add(vt, wasm_f32x4_mul(vn, vln2_lo));

    v128_t vp = wasm_f32x4_mul(vt, vc2);
    vp = wasm_f32x4_sub(vt, wasm_f32x4_mul(vp, vt));

    const v128_t vy = wasm_f32x4_sub(vs, wasm_f32x4_mul(vs, vp));
    const v128_t vd = wasm_f32x4_add(vy, vone);

    v128_t vf = wasm_f32x4_div(vy, vd);
    vf = wasm_v128_andnot(vf, wasm_f32x4_gt(vz, vdenorm_cutoff));
    vf = wasm_v128_bitselect(vf, wasm_f32x4_sub(vone, vf), wasm_i32x4_shr(vx, 31));

    if (n & (2 * sizeof(float))) {
      *((double*) y) = wasm_f64x2_extract_lane(vf, 0);
      vf = wasm_v32x4_shuffle(vf, vf, 2, 3, 2, 3);
      y += 2;
    }
    if (n & (1 * sizeof(float))) {
      *y = wasm_f32x4_extract_lane(vf, 0);
    }
  }
}

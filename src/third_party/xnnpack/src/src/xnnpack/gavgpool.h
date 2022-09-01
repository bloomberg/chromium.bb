// Copyright (c) Facebook, Inc. and its affiliates.
// All rights reserved.
//
// Copyright 2019 Google LLC
//
// This source code is licensed under the BSD-style license found in the
// LICENSE file in the root directory of this source tree.

#pragma once

#include <stddef.h>
#include <stdint.h>

#include <xnnpack/params.h>
#include <xnnpack/common.h>

#ifdef __cplusplus
extern "C" {
#endif


#define DECLARE_F32_GAVGPOOL_MINMAX_MULTIPASS_UKERNEL_FUNCTION(fn_name) \
  XNN_INTERNAL void fn_name(                                            \
      size_t rows,                                                      \
      size_t channels,                                                  \
      const float* input,                                               \
      size_t input_stride,                                              \
      const float* zero,                                                \
      float* buffer,                                                    \
      float* output,                                                    \
      const union xnn_f32_scaleminmax_params* params);

DECLARE_F32_GAVGPOOL_MINMAX_MULTIPASS_UKERNEL_FUNCTION(xnn_f32_gavgpool_minmax_ukernel_7p7x__neon_c4)
DECLARE_F32_GAVGPOOL_MINMAX_MULTIPASS_UKERNEL_FUNCTION(xnn_f32_gavgpool_minmax_ukernel_7p7x__scalar_c1)
DECLARE_F32_GAVGPOOL_MINMAX_MULTIPASS_UKERNEL_FUNCTION(xnn_f32_gavgpool_minmax_ukernel_7p7x__sse_c4)
DECLARE_F32_GAVGPOOL_MINMAX_MULTIPASS_UKERNEL_FUNCTION(xnn_f32_gavgpool_minmax_ukernel_7p7x__wasm_c1)
DECLARE_F32_GAVGPOOL_MINMAX_MULTIPASS_UKERNEL_FUNCTION(xnn_f32_gavgpool_minmax_ukernel_7p7x__wasmsimd_arm_c4)
DECLARE_F32_GAVGPOOL_MINMAX_MULTIPASS_UKERNEL_FUNCTION(xnn_f32_gavgpool_minmax_ukernel_7p7x__wasmsimd_x86_c4)


#define DECLARE_F32_GAVGPOOL_MINMAX_UNIPASS_UKERNEL_FUNCTION(fn_name) \
  XNN_INTERNAL void fn_name(                                          \
      size_t rows,                                                    \
      size_t channels,                                                \
      const float* input,                                             \
      size_t input_stride,                                            \
      const float* zero,                                              \
      float* output,                                                  \
      const union xnn_f32_scaleminmax_params* params);

DECLARE_F32_GAVGPOOL_MINMAX_UNIPASS_UKERNEL_FUNCTION(xnn_f32_gavgpool_minmax_ukernel_7x__neon_c4)
DECLARE_F32_GAVGPOOL_MINMAX_UNIPASS_UKERNEL_FUNCTION(xnn_f32_gavgpool_minmax_ukernel_7x__scalar_c1)
DECLARE_F32_GAVGPOOL_MINMAX_UNIPASS_UKERNEL_FUNCTION(xnn_f32_gavgpool_minmax_ukernel_7x__sse_c4)
DECLARE_F32_GAVGPOOL_MINMAX_UNIPASS_UKERNEL_FUNCTION(xnn_f32_gavgpool_minmax_ukernel_7x__wasm_c1)
DECLARE_F32_GAVGPOOL_MINMAX_UNIPASS_UKERNEL_FUNCTION(xnn_f32_gavgpool_minmax_ukernel_7x__wasmsimd_arm_c4)
DECLARE_F32_GAVGPOOL_MINMAX_UNIPASS_UKERNEL_FUNCTION(xnn_f32_gavgpool_minmax_ukernel_7x__wasmsimd_x86_c4)


#define DECLARE_F16_GAVGPOOL_MINMAX_MULTIPASS_UKERNEL_FUNCTION(fn_name) \
  XNN_INTERNAL void fn_name(                                            \
      size_t rows,                                                      \
      size_t channels,                                                  \
      const void* input,                                                \
      size_t input_stride,                                              \
      const void* zero,                                                 \
      void* buffer,                                                     \
      void* output,                                                     \
      const union xnn_f16_scaleminmax_params* params);

DECLARE_F16_GAVGPOOL_MINMAX_MULTIPASS_UKERNEL_FUNCTION(xnn_f16_gavgpool_minmax_ukernel_7p7x__neonfp16arith_c8)
DECLARE_F16_GAVGPOOL_MINMAX_MULTIPASS_UKERNEL_FUNCTION(xnn_f16_gavgpool_minmax_ukernel_7p7x__neonfp16arith_c16)
DECLARE_F16_GAVGPOOL_MINMAX_MULTIPASS_UKERNEL_FUNCTION(xnn_f16_gavgpool_minmax_ukernel_7p7x__neonfp16arith_c24)
DECLARE_F16_GAVGPOOL_MINMAX_MULTIPASS_UKERNEL_FUNCTION(xnn_f16_gavgpool_minmax_ukernel_7p7x__neonfp16arith_c32)

DECLARE_F16_GAVGPOOL_MINMAX_MULTIPASS_UKERNEL_FUNCTION(xnn_f16_gavgpool_minmax_ukernel_7p7x__f16c_c8)
DECLARE_F16_GAVGPOOL_MINMAX_MULTIPASS_UKERNEL_FUNCTION(xnn_f16_gavgpool_minmax_ukernel_7p7x__f16c_c16)
DECLARE_F16_GAVGPOOL_MINMAX_MULTIPASS_UKERNEL_FUNCTION(xnn_f16_gavgpool_minmax_ukernel_7p7x__f16c_c24)
DECLARE_F16_GAVGPOOL_MINMAX_MULTIPASS_UKERNEL_FUNCTION(xnn_f16_gavgpool_minmax_ukernel_7p7x__f16c_c32)


#define DECLARE_F16_GAVGPOOL_MINMAX_UNIPASS_UKERNEL_FUNCTION(fn_name) \
  XNN_INTERNAL void fn_name(                                          \
      size_t rows,                                                    \
      size_t channels,                                                \
      const void* input,                                              \
      size_t input_stride,                                            \
      const void* zero,                                               \
      void* output,                                                   \
      const union xnn_f16_scaleminmax_params* params);

DECLARE_F16_GAVGPOOL_MINMAX_UNIPASS_UKERNEL_FUNCTION(xnn_f16_gavgpool_minmax_ukernel_7x__neonfp16arith_c8)
DECLARE_F16_GAVGPOOL_MINMAX_UNIPASS_UKERNEL_FUNCTION(xnn_f16_gavgpool_minmax_ukernel_7x__neonfp16arith_c16)
DECLARE_F16_GAVGPOOL_MINMAX_UNIPASS_UKERNEL_FUNCTION(xnn_f16_gavgpool_minmax_ukernel_7x__neonfp16arith_c24)
DECLARE_F16_GAVGPOOL_MINMAX_UNIPASS_UKERNEL_FUNCTION(xnn_f16_gavgpool_minmax_ukernel_7x__neonfp16arith_c32)

DECLARE_F16_GAVGPOOL_MINMAX_UNIPASS_UKERNEL_FUNCTION(xnn_f16_gavgpool_minmax_ukernel_7x__f16c_c8)
DECLARE_F16_GAVGPOOL_MINMAX_UNIPASS_UKERNEL_FUNCTION(xnn_f16_gavgpool_minmax_ukernel_7x__f16c_c16)
DECLARE_F16_GAVGPOOL_MINMAX_UNIPASS_UKERNEL_FUNCTION(xnn_f16_gavgpool_minmax_ukernel_7x__f16c_c24)
DECLARE_F16_GAVGPOOL_MINMAX_UNIPASS_UKERNEL_FUNCTION(xnn_f16_gavgpool_minmax_ukernel_7x__f16c_c32)


#define DECLARE_QS8_GAVGPOOL_MINMAX_MULTIPASS_UKERNEL_FUNCTION(fn_name) \
  XNN_INTERNAL void fn_name(                                            \
      size_t rows,                                                      \
      size_t channels,                                                  \
      const int8_t* input,                                              \
      size_t input_stride,                                              \
      const int8_t* zero,                                               \
      int32_t* buffer,                                                  \
      int8_t* output,                                                   \
      const union xnn_qs8_avgpool_minmax_params* params);

DECLARE_QS8_GAVGPOOL_MINMAX_MULTIPASS_UKERNEL_FUNCTION(xnn_qs8_gavgpool_minmax_fp32_ukernel_7p7x__neon_c8)
DECLARE_QS8_GAVGPOOL_MINMAX_MULTIPASS_UKERNEL_FUNCTION(xnn_qs8_gavgpool_minmax_fp32_ukernel_7p7x__neon_c16)
DECLARE_QS8_GAVGPOOL_MINMAX_MULTIPASS_UKERNEL_FUNCTION(xnn_qs8_gavgpool_minmax_fp32_ukernel_7p7x__neon_c24)
DECLARE_QS8_GAVGPOOL_MINMAX_MULTIPASS_UKERNEL_FUNCTION(xnn_qs8_gavgpool_minmax_fp32_ukernel_7p7x__neon_c32)

DECLARE_QS8_GAVGPOOL_MINMAX_MULTIPASS_UKERNEL_FUNCTION(xnn_qs8_gavgpool_minmax_rndnu_ukernel_7p7x__neon_c8)
DECLARE_QS8_GAVGPOOL_MINMAX_MULTIPASS_UKERNEL_FUNCTION(xnn_qs8_gavgpool_minmax_rndnu_ukernel_7p7x__neon_c16)
DECLARE_QS8_GAVGPOOL_MINMAX_MULTIPASS_UKERNEL_FUNCTION(xnn_qs8_gavgpool_minmax_rndnu_ukernel_7p7x__neon_c24)
DECLARE_QS8_GAVGPOOL_MINMAX_MULTIPASS_UKERNEL_FUNCTION(xnn_qs8_gavgpool_minmax_rndnu_ukernel_7p7x__neon_c32)

DECLARE_QS8_GAVGPOOL_MINMAX_MULTIPASS_UKERNEL_FUNCTION(xnn_qs8_gavgpool_minmax_fp32_ukernel_7p7x__neonv8_c8)
DECLARE_QS8_GAVGPOOL_MINMAX_MULTIPASS_UKERNEL_FUNCTION(xnn_qs8_gavgpool_minmax_fp32_ukernel_7p7x__neonv8_c16)
DECLARE_QS8_GAVGPOOL_MINMAX_MULTIPASS_UKERNEL_FUNCTION(xnn_qs8_gavgpool_minmax_fp32_ukernel_7p7x__neonv8_c24)
DECLARE_QS8_GAVGPOOL_MINMAX_MULTIPASS_UKERNEL_FUNCTION(xnn_qs8_gavgpool_minmax_fp32_ukernel_7p7x__neonv8_c32)

DECLARE_QS8_GAVGPOOL_MINMAX_MULTIPASS_UKERNEL_FUNCTION(xnn_qs8_gavgpool_minmax_fp32_ukernel_7p7x__sse2_c8)
DECLARE_QS8_GAVGPOOL_MINMAX_MULTIPASS_UKERNEL_FUNCTION(xnn_qs8_gavgpool_minmax_fp32_ukernel_7p7x__sse2_c16)
DECLARE_QS8_GAVGPOOL_MINMAX_MULTIPASS_UKERNEL_FUNCTION(xnn_qs8_gavgpool_minmax_fp32_ukernel_7p7x__sse2_c24)

DECLARE_QS8_GAVGPOOL_MINMAX_MULTIPASS_UKERNEL_FUNCTION(xnn_qs8_gavgpool_minmax_fp32_ukernel_7p7x__sse41_c8)
DECLARE_QS8_GAVGPOOL_MINMAX_MULTIPASS_UKERNEL_FUNCTION(xnn_qs8_gavgpool_minmax_fp32_ukernel_7p7x__sse41_c16)
DECLARE_QS8_GAVGPOOL_MINMAX_MULTIPASS_UKERNEL_FUNCTION(xnn_qs8_gavgpool_minmax_fp32_ukernel_7p7x__sse41_c24)

DECLARE_QS8_GAVGPOOL_MINMAX_MULTIPASS_UKERNEL_FUNCTION(xnn_qs8_gavgpool_minmax_fp32_ukernel_7p7x__wasmsimd_c8)
DECLARE_QS8_GAVGPOOL_MINMAX_MULTIPASS_UKERNEL_FUNCTION(xnn_qs8_gavgpool_minmax_fp32_ukernel_7p7x__wasmsimd_c16)
DECLARE_QS8_GAVGPOOL_MINMAX_MULTIPASS_UKERNEL_FUNCTION(xnn_qs8_gavgpool_minmax_fp32_ukernel_7p7x__wasmsimd_c24)
DECLARE_QS8_GAVGPOOL_MINMAX_MULTIPASS_UKERNEL_FUNCTION(xnn_qs8_gavgpool_minmax_fp32_ukernel_7p7x__wasmsimd_c32)

DECLARE_QS8_GAVGPOOL_MINMAX_MULTIPASS_UKERNEL_FUNCTION(xnn_qs8_gavgpool_minmax_fp32_ukernel_7p7x__scalar_imagic_c1)
DECLARE_QS8_GAVGPOOL_MINMAX_MULTIPASS_UKERNEL_FUNCTION(xnn_qs8_gavgpool_minmax_fp32_ukernel_7p7x__scalar_imagic_c2)
DECLARE_QS8_GAVGPOOL_MINMAX_MULTIPASS_UKERNEL_FUNCTION(xnn_qs8_gavgpool_minmax_fp32_ukernel_7p7x__scalar_imagic_c4)

DECLARE_QS8_GAVGPOOL_MINMAX_MULTIPASS_UKERNEL_FUNCTION(xnn_qs8_gavgpool_minmax_fp32_ukernel_7p7x__scalar_fmagic_c1)
DECLARE_QS8_GAVGPOOL_MINMAX_MULTIPASS_UKERNEL_FUNCTION(xnn_qs8_gavgpool_minmax_fp32_ukernel_7p7x__scalar_fmagic_c2)
DECLARE_QS8_GAVGPOOL_MINMAX_MULTIPASS_UKERNEL_FUNCTION(xnn_qs8_gavgpool_minmax_fp32_ukernel_7p7x__scalar_fmagic_c4)

DECLARE_QS8_GAVGPOOL_MINMAX_MULTIPASS_UKERNEL_FUNCTION(xnn_qs8_gavgpool_minmax_fp32_ukernel_7p7x__scalar_lrintf_c1)
DECLARE_QS8_GAVGPOOL_MINMAX_MULTIPASS_UKERNEL_FUNCTION(xnn_qs8_gavgpool_minmax_fp32_ukernel_7p7x__scalar_lrintf_c2)
DECLARE_QS8_GAVGPOOL_MINMAX_MULTIPASS_UKERNEL_FUNCTION(xnn_qs8_gavgpool_minmax_fp32_ukernel_7p7x__scalar_lrintf_c4)


#define DECLARE_QS8_GAVGPOOL_MINMAX_UNIPASS_UKERNEL_FUNCTION(fn_name) \
  XNN_INTERNAL void fn_name(                                          \
      size_t rows,                                                    \
      size_t channels,                                                \
      const int8_t* input,                                            \
      size_t input_stride,                                            \
      const int8_t* zero,                                             \
      int8_t* output,                                                 \
      const union xnn_qs8_avgpool_minmax_params* params);

DECLARE_QS8_GAVGPOOL_MINMAX_UNIPASS_UKERNEL_FUNCTION(xnn_qs8_gavgpool_minmax_fp32_ukernel_7x__neon_c8)
DECLARE_QS8_GAVGPOOL_MINMAX_UNIPASS_UKERNEL_FUNCTION(xnn_qs8_gavgpool_minmax_fp32_ukernel_7x__neon_c16)
DECLARE_QS8_GAVGPOOL_MINMAX_UNIPASS_UKERNEL_FUNCTION(xnn_qs8_gavgpool_minmax_fp32_ukernel_7x__neon_c24)
DECLARE_QS8_GAVGPOOL_MINMAX_UNIPASS_UKERNEL_FUNCTION(xnn_qs8_gavgpool_minmax_fp32_ukernel_7x__neon_c32)

DECLARE_QS8_GAVGPOOL_MINMAX_UNIPASS_UKERNEL_FUNCTION(xnn_qs8_gavgpool_minmax_rndnu_ukernel_7x__neon_c8)
DECLARE_QS8_GAVGPOOL_MINMAX_UNIPASS_UKERNEL_FUNCTION(xnn_qs8_gavgpool_minmax_rndnu_ukernel_7x__neon_c16)
DECLARE_QS8_GAVGPOOL_MINMAX_UNIPASS_UKERNEL_FUNCTION(xnn_qs8_gavgpool_minmax_rndnu_ukernel_7x__neon_c24)
DECLARE_QS8_GAVGPOOL_MINMAX_UNIPASS_UKERNEL_FUNCTION(xnn_qs8_gavgpool_minmax_rndnu_ukernel_7x__neon_c32)

DECLARE_QS8_GAVGPOOL_MINMAX_UNIPASS_UKERNEL_FUNCTION(xnn_qs8_gavgpool_minmax_fp32_ukernel_7x__neonv8_c8)
DECLARE_QS8_GAVGPOOL_MINMAX_UNIPASS_UKERNEL_FUNCTION(xnn_qs8_gavgpool_minmax_fp32_ukernel_7x__neonv8_c16)
DECLARE_QS8_GAVGPOOL_MINMAX_UNIPASS_UKERNEL_FUNCTION(xnn_qs8_gavgpool_minmax_fp32_ukernel_7x__neonv8_c24)
DECLARE_QS8_GAVGPOOL_MINMAX_UNIPASS_UKERNEL_FUNCTION(xnn_qs8_gavgpool_minmax_fp32_ukernel_7x__neonv8_c32)

DECLARE_QS8_GAVGPOOL_MINMAX_UNIPASS_UKERNEL_FUNCTION(xnn_qs8_gavgpool_minmax_fp32_ukernel_7x__sse2_c8)
DECLARE_QS8_GAVGPOOL_MINMAX_UNIPASS_UKERNEL_FUNCTION(xnn_qs8_gavgpool_minmax_fp32_ukernel_7x__sse2_c16)
DECLARE_QS8_GAVGPOOL_MINMAX_UNIPASS_UKERNEL_FUNCTION(xnn_qs8_gavgpool_minmax_fp32_ukernel_7x__sse2_c24)

DECLARE_QS8_GAVGPOOL_MINMAX_UNIPASS_UKERNEL_FUNCTION(xnn_qs8_gavgpool_minmax_fp32_ukernel_7x__sse41_c8)
DECLARE_QS8_GAVGPOOL_MINMAX_UNIPASS_UKERNEL_FUNCTION(xnn_qs8_gavgpool_minmax_fp32_ukernel_7x__sse41_c16)
DECLARE_QS8_GAVGPOOL_MINMAX_UNIPASS_UKERNEL_FUNCTION(xnn_qs8_gavgpool_minmax_fp32_ukernel_7x__sse41_c24)

DECLARE_QS8_GAVGPOOL_MINMAX_UNIPASS_UKERNEL_FUNCTION(xnn_qs8_gavgpool_minmax_fp32_ukernel_7x__wasmsimd_c8)
DECLARE_QS8_GAVGPOOL_MINMAX_UNIPASS_UKERNEL_FUNCTION(xnn_qs8_gavgpool_minmax_fp32_ukernel_7x__wasmsimd_c16)
DECLARE_QS8_GAVGPOOL_MINMAX_UNIPASS_UKERNEL_FUNCTION(xnn_qs8_gavgpool_minmax_fp32_ukernel_7x__wasmsimd_c24)
DECLARE_QS8_GAVGPOOL_MINMAX_UNIPASS_UKERNEL_FUNCTION(xnn_qs8_gavgpool_minmax_fp32_ukernel_7x__wasmsimd_c32)

DECLARE_QS8_GAVGPOOL_MINMAX_UNIPASS_UKERNEL_FUNCTION(xnn_qs8_gavgpool_minmax_fp32_ukernel_7x__scalar_imagic_c1)
DECLARE_QS8_GAVGPOOL_MINMAX_UNIPASS_UKERNEL_FUNCTION(xnn_qs8_gavgpool_minmax_fp32_ukernel_7x__scalar_imagic_c2)
DECLARE_QS8_GAVGPOOL_MINMAX_UNIPASS_UKERNEL_FUNCTION(xnn_qs8_gavgpool_minmax_fp32_ukernel_7x__scalar_imagic_c4)

DECLARE_QS8_GAVGPOOL_MINMAX_UNIPASS_UKERNEL_FUNCTION(xnn_qs8_gavgpool_minmax_fp32_ukernel_7x__scalar_fmagic_c1)
DECLARE_QS8_GAVGPOOL_MINMAX_UNIPASS_UKERNEL_FUNCTION(xnn_qs8_gavgpool_minmax_fp32_ukernel_7x__scalar_fmagic_c2)
DECLARE_QS8_GAVGPOOL_MINMAX_UNIPASS_UKERNEL_FUNCTION(xnn_qs8_gavgpool_minmax_fp32_ukernel_7x__scalar_fmagic_c4)

DECLARE_QS8_GAVGPOOL_MINMAX_UNIPASS_UKERNEL_FUNCTION(xnn_qs8_gavgpool_minmax_fp32_ukernel_7x__scalar_lrintf_c1)
DECLARE_QS8_GAVGPOOL_MINMAX_UNIPASS_UKERNEL_FUNCTION(xnn_qs8_gavgpool_minmax_fp32_ukernel_7x__scalar_lrintf_c2)
DECLARE_QS8_GAVGPOOL_MINMAX_UNIPASS_UKERNEL_FUNCTION(xnn_qs8_gavgpool_minmax_fp32_ukernel_7x__scalar_lrintf_c4)


#define DECLARE_QU8_GAVGPOOL_MINMAX_MULTIPASS_UKERNEL_FUNCTION(fn_name) \
  XNN_INTERNAL void fn_name(                                            \
      size_t rows,                                                      \
      size_t channels,                                                  \
      const uint8_t* input,                                             \
      size_t input_stride,                                              \
      const uint8_t* zero,                                              \
      int32_t* buffer,                                                  \
      uint8_t* output,                                                  \
      const union xnn_qu8_avgpool_minmax_params* params);

DECLARE_QU8_GAVGPOOL_MINMAX_MULTIPASS_UKERNEL_FUNCTION(xnn_qu8_gavgpool_minmax_fp32_ukernel_7p7x__neon_c8)
DECLARE_QU8_GAVGPOOL_MINMAX_MULTIPASS_UKERNEL_FUNCTION(xnn_qu8_gavgpool_minmax_fp32_ukernel_7p7x__neon_c16)
DECLARE_QU8_GAVGPOOL_MINMAX_MULTIPASS_UKERNEL_FUNCTION(xnn_qu8_gavgpool_minmax_fp32_ukernel_7p7x__neon_c24)
DECLARE_QU8_GAVGPOOL_MINMAX_MULTIPASS_UKERNEL_FUNCTION(xnn_qu8_gavgpool_minmax_fp32_ukernel_7p7x__neon_c32)

DECLARE_QU8_GAVGPOOL_MINMAX_MULTIPASS_UKERNEL_FUNCTION(xnn_qu8_gavgpool_minmax_rndnu_ukernel_7p7x__neon_c8)
DECLARE_QU8_GAVGPOOL_MINMAX_MULTIPASS_UKERNEL_FUNCTION(xnn_qu8_gavgpool_minmax_rndnu_ukernel_7p7x__neon_c16)
DECLARE_QU8_GAVGPOOL_MINMAX_MULTIPASS_UKERNEL_FUNCTION(xnn_qu8_gavgpool_minmax_rndnu_ukernel_7p7x__neon_c24)
DECLARE_QU8_GAVGPOOL_MINMAX_MULTIPASS_UKERNEL_FUNCTION(xnn_qu8_gavgpool_minmax_rndnu_ukernel_7p7x__neon_c32)

DECLARE_QU8_GAVGPOOL_MINMAX_MULTIPASS_UKERNEL_FUNCTION(xnn_qu8_gavgpool_minmax_fp32_ukernel_7p7x__neonv8_c8)
DECLARE_QU8_GAVGPOOL_MINMAX_MULTIPASS_UKERNEL_FUNCTION(xnn_qu8_gavgpool_minmax_fp32_ukernel_7p7x__neonv8_c16)
DECLARE_QU8_GAVGPOOL_MINMAX_MULTIPASS_UKERNEL_FUNCTION(xnn_qu8_gavgpool_minmax_fp32_ukernel_7p7x__neonv8_c24)
DECLARE_QU8_GAVGPOOL_MINMAX_MULTIPASS_UKERNEL_FUNCTION(xnn_qu8_gavgpool_minmax_fp32_ukernel_7p7x__neonv8_c32)

DECLARE_QU8_GAVGPOOL_MINMAX_MULTIPASS_UKERNEL_FUNCTION(xnn_qu8_gavgpool_minmax_fp32_ukernel_7p7x__sse2_c8)
DECLARE_QU8_GAVGPOOL_MINMAX_MULTIPASS_UKERNEL_FUNCTION(xnn_qu8_gavgpool_minmax_fp32_ukernel_7p7x__sse2_c16)
DECLARE_QU8_GAVGPOOL_MINMAX_MULTIPASS_UKERNEL_FUNCTION(xnn_qu8_gavgpool_minmax_fp32_ukernel_7p7x__sse2_c24)

DECLARE_QU8_GAVGPOOL_MINMAX_MULTIPASS_UKERNEL_FUNCTION(xnn_qu8_gavgpool_minmax_fp32_ukernel_7p7x__sse41_c8)
DECLARE_QU8_GAVGPOOL_MINMAX_MULTIPASS_UKERNEL_FUNCTION(xnn_qu8_gavgpool_minmax_fp32_ukernel_7p7x__sse41_c16)
DECLARE_QU8_GAVGPOOL_MINMAX_MULTIPASS_UKERNEL_FUNCTION(xnn_qu8_gavgpool_minmax_fp32_ukernel_7p7x__sse41_c24)

DECLARE_QU8_GAVGPOOL_MINMAX_MULTIPASS_UKERNEL_FUNCTION(xnn_qu8_gavgpool_minmax_fp32_ukernel_7p7x__wasmsimd_c8)
DECLARE_QU8_GAVGPOOL_MINMAX_MULTIPASS_UKERNEL_FUNCTION(xnn_qu8_gavgpool_minmax_fp32_ukernel_7p7x__wasmsimd_c16)
DECLARE_QU8_GAVGPOOL_MINMAX_MULTIPASS_UKERNEL_FUNCTION(xnn_qu8_gavgpool_minmax_fp32_ukernel_7p7x__wasmsimd_c24)
DECLARE_QU8_GAVGPOOL_MINMAX_MULTIPASS_UKERNEL_FUNCTION(xnn_qu8_gavgpool_minmax_fp32_ukernel_7p7x__wasmsimd_c32)

DECLARE_QU8_GAVGPOOL_MINMAX_MULTIPASS_UKERNEL_FUNCTION(xnn_qu8_gavgpool_minmax_fp32_ukernel_7p7x__scalar_imagic_c1)
DECLARE_QU8_GAVGPOOL_MINMAX_MULTIPASS_UKERNEL_FUNCTION(xnn_qu8_gavgpool_minmax_fp32_ukernel_7p7x__scalar_imagic_c2)
DECLARE_QU8_GAVGPOOL_MINMAX_MULTIPASS_UKERNEL_FUNCTION(xnn_qu8_gavgpool_minmax_fp32_ukernel_7p7x__scalar_imagic_c4)

DECLARE_QU8_GAVGPOOL_MINMAX_MULTIPASS_UKERNEL_FUNCTION(xnn_qu8_gavgpool_minmax_fp32_ukernel_7p7x__scalar_fmagic_c1)
DECLARE_QU8_GAVGPOOL_MINMAX_MULTIPASS_UKERNEL_FUNCTION(xnn_qu8_gavgpool_minmax_fp32_ukernel_7p7x__scalar_fmagic_c2)
DECLARE_QU8_GAVGPOOL_MINMAX_MULTIPASS_UKERNEL_FUNCTION(xnn_qu8_gavgpool_minmax_fp32_ukernel_7p7x__scalar_fmagic_c4)

DECLARE_QU8_GAVGPOOL_MINMAX_MULTIPASS_UKERNEL_FUNCTION(xnn_qu8_gavgpool_minmax_fp32_ukernel_7p7x__scalar_lrintf_c1)
DECLARE_QU8_GAVGPOOL_MINMAX_MULTIPASS_UKERNEL_FUNCTION(xnn_qu8_gavgpool_minmax_fp32_ukernel_7p7x__scalar_lrintf_c2)
DECLARE_QU8_GAVGPOOL_MINMAX_MULTIPASS_UKERNEL_FUNCTION(xnn_qu8_gavgpool_minmax_fp32_ukernel_7p7x__scalar_lrintf_c4)


#define DECLARE_QU8_GAVGPOOL_MINMAX_UNIPASS_UKERNEL_FUNCTION(fn_name) \
  XNN_INTERNAL void fn_name(                                          \
      size_t rows,                                                    \
      size_t channels,                                                \
      const uint8_t* input,                                           \
      size_t input_stride,                                            \
      const uint8_t* zero,                                            \
      uint8_t* output,                                                \
      const union xnn_qu8_avgpool_minmax_params* params);

DECLARE_QU8_GAVGPOOL_MINMAX_UNIPASS_UKERNEL_FUNCTION(xnn_qu8_gavgpool_minmax_fp32_ukernel_7x__neon_c8)
DECLARE_QU8_GAVGPOOL_MINMAX_UNIPASS_UKERNEL_FUNCTION(xnn_qu8_gavgpool_minmax_fp32_ukernel_7x__neon_c16)
DECLARE_QU8_GAVGPOOL_MINMAX_UNIPASS_UKERNEL_FUNCTION(xnn_qu8_gavgpool_minmax_fp32_ukernel_7x__neon_c24)
DECLARE_QU8_GAVGPOOL_MINMAX_UNIPASS_UKERNEL_FUNCTION(xnn_qu8_gavgpool_minmax_fp32_ukernel_7x__neon_c32)

DECLARE_QU8_GAVGPOOL_MINMAX_UNIPASS_UKERNEL_FUNCTION(xnn_qu8_gavgpool_minmax_rndnu_ukernel_7x__neon_c8)
DECLARE_QU8_GAVGPOOL_MINMAX_UNIPASS_UKERNEL_FUNCTION(xnn_qu8_gavgpool_minmax_rndnu_ukernel_7x__neon_c16)
DECLARE_QU8_GAVGPOOL_MINMAX_UNIPASS_UKERNEL_FUNCTION(xnn_qu8_gavgpool_minmax_rndnu_ukernel_7x__neon_c24)
DECLARE_QU8_GAVGPOOL_MINMAX_UNIPASS_UKERNEL_FUNCTION(xnn_qu8_gavgpool_minmax_rndnu_ukernel_7x__neon_c32)

DECLARE_QU8_GAVGPOOL_MINMAX_UNIPASS_UKERNEL_FUNCTION(xnn_qu8_gavgpool_minmax_fp32_ukernel_7x__neonv8_c8)
DECLARE_QU8_GAVGPOOL_MINMAX_UNIPASS_UKERNEL_FUNCTION(xnn_qu8_gavgpool_minmax_fp32_ukernel_7x__neonv8_c16)
DECLARE_QU8_GAVGPOOL_MINMAX_UNIPASS_UKERNEL_FUNCTION(xnn_qu8_gavgpool_minmax_fp32_ukernel_7x__neonv8_c24)
DECLARE_QU8_GAVGPOOL_MINMAX_UNIPASS_UKERNEL_FUNCTION(xnn_qu8_gavgpool_minmax_fp32_ukernel_7x__neonv8_c32)

DECLARE_QU8_GAVGPOOL_MINMAX_UNIPASS_UKERNEL_FUNCTION(xnn_qu8_gavgpool_minmax_fp32_ukernel_7x__sse2_c8)
DECLARE_QU8_GAVGPOOL_MINMAX_UNIPASS_UKERNEL_FUNCTION(xnn_qu8_gavgpool_minmax_fp32_ukernel_7x__sse2_c16)
DECLARE_QU8_GAVGPOOL_MINMAX_UNIPASS_UKERNEL_FUNCTION(xnn_qu8_gavgpool_minmax_fp32_ukernel_7x__sse2_c24)

DECLARE_QU8_GAVGPOOL_MINMAX_UNIPASS_UKERNEL_FUNCTION(xnn_qu8_gavgpool_minmax_fp32_ukernel_7x__sse41_c8)
DECLARE_QU8_GAVGPOOL_MINMAX_UNIPASS_UKERNEL_FUNCTION(xnn_qu8_gavgpool_minmax_fp32_ukernel_7x__sse41_c16)
DECLARE_QU8_GAVGPOOL_MINMAX_UNIPASS_UKERNEL_FUNCTION(xnn_qu8_gavgpool_minmax_fp32_ukernel_7x__sse41_c24)

DECLARE_QU8_GAVGPOOL_MINMAX_UNIPASS_UKERNEL_FUNCTION(xnn_qu8_gavgpool_minmax_fp32_ukernel_7x__wasmsimd_c8)
DECLARE_QU8_GAVGPOOL_MINMAX_UNIPASS_UKERNEL_FUNCTION(xnn_qu8_gavgpool_minmax_fp32_ukernel_7x__wasmsimd_c16)
DECLARE_QU8_GAVGPOOL_MINMAX_UNIPASS_UKERNEL_FUNCTION(xnn_qu8_gavgpool_minmax_fp32_ukernel_7x__wasmsimd_c24)
DECLARE_QU8_GAVGPOOL_MINMAX_UNIPASS_UKERNEL_FUNCTION(xnn_qu8_gavgpool_minmax_fp32_ukernel_7x__wasmsimd_c32)

DECLARE_QU8_GAVGPOOL_MINMAX_UNIPASS_UKERNEL_FUNCTION(xnn_qu8_gavgpool_minmax_fp32_ukernel_7x__scalar_imagic_c1)
DECLARE_QU8_GAVGPOOL_MINMAX_UNIPASS_UKERNEL_FUNCTION(xnn_qu8_gavgpool_minmax_fp32_ukernel_7x__scalar_imagic_c2)
DECLARE_QU8_GAVGPOOL_MINMAX_UNIPASS_UKERNEL_FUNCTION(xnn_qu8_gavgpool_minmax_fp32_ukernel_7x__scalar_imagic_c4)

DECLARE_QU8_GAVGPOOL_MINMAX_UNIPASS_UKERNEL_FUNCTION(xnn_qu8_gavgpool_minmax_fp32_ukernel_7x__scalar_fmagic_c1)
DECLARE_QU8_GAVGPOOL_MINMAX_UNIPASS_UKERNEL_FUNCTION(xnn_qu8_gavgpool_minmax_fp32_ukernel_7x__scalar_fmagic_c2)
DECLARE_QU8_GAVGPOOL_MINMAX_UNIPASS_UKERNEL_FUNCTION(xnn_qu8_gavgpool_minmax_fp32_ukernel_7x__scalar_fmagic_c4)

DECLARE_QU8_GAVGPOOL_MINMAX_UNIPASS_UKERNEL_FUNCTION(xnn_qu8_gavgpool_minmax_fp32_ukernel_7x__scalar_lrintf_c1)
DECLARE_QU8_GAVGPOOL_MINMAX_UNIPASS_UKERNEL_FUNCTION(xnn_qu8_gavgpool_minmax_fp32_ukernel_7x__scalar_lrintf_c2)
DECLARE_QU8_GAVGPOOL_MINMAX_UNIPASS_UKERNEL_FUNCTION(xnn_qu8_gavgpool_minmax_fp32_ukernel_7x__scalar_lrintf_c4)


#define DECLARE_F32_GAVGPOOL_CW_UKERNEL_FUNCTION(fn_name) \
  XNN_INTERNAL void fn_name(                              \
      size_t elements,                                    \
      size_t channels,                                    \
      const float* input,                                 \
      float* output,                                      \
      const union xnn_f32_gavgpool_params* params);

DECLARE_F32_GAVGPOOL_CW_UKERNEL_FUNCTION(xnn_f32_gavgpool_cw_ukernel__neon_x4)
DECLARE_F32_GAVGPOOL_CW_UKERNEL_FUNCTION(xnn_f32_gavgpool_cw_ukernel__scalar_x1)
DECLARE_F32_GAVGPOOL_CW_UKERNEL_FUNCTION(xnn_f32_gavgpool_cw_ukernel__sse_x4)
DECLARE_F32_GAVGPOOL_CW_UKERNEL_FUNCTION(xnn_f32_gavgpool_cw_ukernel__wasmsimd_arm_x4)
DECLARE_F32_GAVGPOOL_CW_UKERNEL_FUNCTION(xnn_f32_gavgpool_cw_ukernel__wasmsimd_x86_x4)


#define DECLARE_F16_GAVGPOOL_CW_UKERNEL_FUNCTION(fn_name) \
  XNN_INTERNAL void fn_name(                              \
      size_t elements,                                    \
      size_t channels,                                    \
      const void* input,                                  \
      void* output,                                       \
      const union xnn_f16_gavgpool_params* params);

DECLARE_F16_GAVGPOOL_CW_UKERNEL_FUNCTION(xnn_f16_gavgpool_cw_ukernel__neonfp16arith_x4)
DECLARE_F16_GAVGPOOL_CW_UKERNEL_FUNCTION(xnn_f16_gavgpool_cw_ukernel__neonfp16arith_x8)


#ifdef __cplusplus
}  // extern "C"
#endif

/*===---- arm_mve.h - ARM MVE intrinsics -----------------------------------===
 *
 *
 * Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
 * See https://llvm.org/LICENSE.txt for license information.
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 *
 *===-----------------------------------------------------------------------===
 */

#ifndef __ARM_MVE_H
#define __ARM_MVE_H

#if !__ARM_FEATURE_MVE
#error "MVE support not enabled"
#endif

#include <stdint.h>

typedef uint16_t mve_pred16_t;
typedef __attribute__((neon_vector_type(8))) int16_t int16x8_t;
typedef struct { int16x8_t val[2]; } int16x8x2_t;
typedef struct { int16x8_t val[4]; } int16x8x4_t;
typedef __attribute__((neon_vector_type(4))) int32_t int32x4_t;
typedef struct { int32x4_t val[2]; } int32x4x2_t;
typedef struct { int32x4_t val[4]; } int32x4x4_t;
typedef __attribute__((neon_vector_type(2))) int64_t int64x2_t;
typedef struct { int64x2_t val[2]; } int64x2x2_t;
typedef struct { int64x2_t val[4]; } int64x2x4_t;
typedef __attribute__((neon_vector_type(16))) int8_t int8x16_t;
typedef struct { int8x16_t val[2]; } int8x16x2_t;
typedef struct { int8x16_t val[4]; } int8x16x4_t;
typedef __attribute__((neon_vector_type(8))) uint16_t uint16x8_t;
typedef struct { uint16x8_t val[2]; } uint16x8x2_t;
typedef struct { uint16x8_t val[4]; } uint16x8x4_t;
typedef __attribute__((neon_vector_type(4))) uint32_t uint32x4_t;
typedef struct { uint32x4_t val[2]; } uint32x4x2_t;
typedef struct { uint32x4_t val[4]; } uint32x4x4_t;
typedef __attribute__((neon_vector_type(2))) uint64_t uint64x2_t;
typedef struct { uint64x2_t val[2]; } uint64x2x2_t;
typedef struct { uint64x2_t val[4]; } uint64x2x4_t;
typedef __attribute__((neon_vector_type(16))) uint8_t uint8x16_t;
typedef struct { uint8x16_t val[2]; } uint8x16x2_t;
typedef struct { uint8x16_t val[4]; } uint8x16x4_t;

static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_asrl)))
int64_t __arm_asrl(int64_t, int32_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_lsll)))
uint64_t __arm_lsll(uint64_t, int32_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_sqrshr)))
int32_t __arm_sqrshr(int32_t, int32_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_sqrshrl)))
int64_t __arm_sqrshrl(int64_t, int32_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_sqrshrl_sat48)))
int64_t __arm_sqrshrl_sat48(int64_t, int32_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_sqshl)))
int32_t __arm_sqshl(int32_t, int);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_sqshll)))
int64_t __arm_sqshll(int64_t, int);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_srshr)))
int32_t __arm_srshr(int32_t, int);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_srshrl)))
int64_t __arm_srshrl(int64_t, int);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_uqrshl)))
uint32_t __arm_uqrshl(uint32_t, int32_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_uqrshll)))
uint64_t __arm_uqrshll(uint64_t, int32_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_uqrshll_sat48)))
uint64_t __arm_uqrshll_sat48(uint64_t, int32_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_uqshl)))
uint32_t __arm_uqshl(uint32_t, int);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_uqshll)))
uint64_t __arm_uqshll(uint64_t, int);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_urshr)))
uint32_t __arm_urshr(uint32_t, int);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_urshrl)))
uint64_t __arm_urshrl(uint64_t, int);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vabdq_m_s16)))
int16x8_t __arm_vabdq_m_s16(int16x8_t, int16x8_t, int16x8_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vabdq_m_s16)))
int16x8_t __arm_vabdq_m(int16x8_t, int16x8_t, int16x8_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vabdq_m_s32)))
int32x4_t __arm_vabdq_m_s32(int32x4_t, int32x4_t, int32x4_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vabdq_m_s32)))
int32x4_t __arm_vabdq_m(int32x4_t, int32x4_t, int32x4_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vabdq_m_s8)))
int8x16_t __arm_vabdq_m_s8(int8x16_t, int8x16_t, int8x16_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vabdq_m_s8)))
int8x16_t __arm_vabdq_m(int8x16_t, int8x16_t, int8x16_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vabdq_m_u16)))
uint16x8_t __arm_vabdq_m_u16(uint16x8_t, uint16x8_t, uint16x8_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vabdq_m_u16)))
uint16x8_t __arm_vabdq_m(uint16x8_t, uint16x8_t, uint16x8_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vabdq_m_u32)))
uint32x4_t __arm_vabdq_m_u32(uint32x4_t, uint32x4_t, uint32x4_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vabdq_m_u32)))
uint32x4_t __arm_vabdq_m(uint32x4_t, uint32x4_t, uint32x4_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vabdq_m_u8)))
uint8x16_t __arm_vabdq_m_u8(uint8x16_t, uint8x16_t, uint8x16_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vabdq_m_u8)))
uint8x16_t __arm_vabdq_m(uint8x16_t, uint8x16_t, uint8x16_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vabdq_s16)))
int16x8_t __arm_vabdq_s16(int16x8_t, int16x8_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vabdq_s16)))
int16x8_t __arm_vabdq(int16x8_t, int16x8_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vabdq_s32)))
int32x4_t __arm_vabdq_s32(int32x4_t, int32x4_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vabdq_s32)))
int32x4_t __arm_vabdq(int32x4_t, int32x4_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vabdq_s8)))
int8x16_t __arm_vabdq_s8(int8x16_t, int8x16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vabdq_s8)))
int8x16_t __arm_vabdq(int8x16_t, int8x16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vabdq_u16)))
uint16x8_t __arm_vabdq_u16(uint16x8_t, uint16x8_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vabdq_u16)))
uint16x8_t __arm_vabdq(uint16x8_t, uint16x8_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vabdq_u32)))
uint32x4_t __arm_vabdq_u32(uint32x4_t, uint32x4_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vabdq_u32)))
uint32x4_t __arm_vabdq(uint32x4_t, uint32x4_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vabdq_u8)))
uint8x16_t __arm_vabdq_u8(uint8x16_t, uint8x16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vabdq_u8)))
uint8x16_t __arm_vabdq(uint8x16_t, uint8x16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vadciq_m_s32)))
int32x4_t __arm_vadciq_m_s32(int32x4_t, int32x4_t, int32x4_t, unsigned *, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vadciq_m_s32)))
int32x4_t __arm_vadciq_m(int32x4_t, int32x4_t, int32x4_t, unsigned *, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vadciq_m_u32)))
uint32x4_t __arm_vadciq_m_u32(uint32x4_t, uint32x4_t, uint32x4_t, unsigned *, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vadciq_m_u32)))
uint32x4_t __arm_vadciq_m(uint32x4_t, uint32x4_t, uint32x4_t, unsigned *, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vadciq_s32)))
int32x4_t __arm_vadciq_s32(int32x4_t, int32x4_t, unsigned *);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vadciq_s32)))
int32x4_t __arm_vadciq(int32x4_t, int32x4_t, unsigned *);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vadciq_u32)))
uint32x4_t __arm_vadciq_u32(uint32x4_t, uint32x4_t, unsigned *);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vadciq_u32)))
uint32x4_t __arm_vadciq(uint32x4_t, uint32x4_t, unsigned *);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vadcq_m_s32)))
int32x4_t __arm_vadcq_m_s32(int32x4_t, int32x4_t, int32x4_t, unsigned *, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vadcq_m_s32)))
int32x4_t __arm_vadcq_m(int32x4_t, int32x4_t, int32x4_t, unsigned *, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vadcq_m_u32)))
uint32x4_t __arm_vadcq_m_u32(uint32x4_t, uint32x4_t, uint32x4_t, unsigned *, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vadcq_m_u32)))
uint32x4_t __arm_vadcq_m(uint32x4_t, uint32x4_t, uint32x4_t, unsigned *, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vadcq_s32)))
int32x4_t __arm_vadcq_s32(int32x4_t, int32x4_t, unsigned *);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vadcq_s32)))
int32x4_t __arm_vadcq(int32x4_t, int32x4_t, unsigned *);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vadcq_u32)))
uint32x4_t __arm_vadcq_u32(uint32x4_t, uint32x4_t, unsigned *);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vadcq_u32)))
uint32x4_t __arm_vadcq(uint32x4_t, uint32x4_t, unsigned *);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vaddq_m_s16)))
int16x8_t __arm_vaddq_m_s16(int16x8_t, int16x8_t, int16x8_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vaddq_m_s16)))
int16x8_t __arm_vaddq_m(int16x8_t, int16x8_t, int16x8_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vaddq_m_s32)))
int32x4_t __arm_vaddq_m_s32(int32x4_t, int32x4_t, int32x4_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vaddq_m_s32)))
int32x4_t __arm_vaddq_m(int32x4_t, int32x4_t, int32x4_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vaddq_m_s8)))
int8x16_t __arm_vaddq_m_s8(int8x16_t, int8x16_t, int8x16_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vaddq_m_s8)))
int8x16_t __arm_vaddq_m(int8x16_t, int8x16_t, int8x16_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vaddq_m_u16)))
uint16x8_t __arm_vaddq_m_u16(uint16x8_t, uint16x8_t, uint16x8_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vaddq_m_u16)))
uint16x8_t __arm_vaddq_m(uint16x8_t, uint16x8_t, uint16x8_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vaddq_m_u32)))
uint32x4_t __arm_vaddq_m_u32(uint32x4_t, uint32x4_t, uint32x4_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vaddq_m_u32)))
uint32x4_t __arm_vaddq_m(uint32x4_t, uint32x4_t, uint32x4_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vaddq_m_u8)))
uint8x16_t __arm_vaddq_m_u8(uint8x16_t, uint8x16_t, uint8x16_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vaddq_m_u8)))
uint8x16_t __arm_vaddq_m(uint8x16_t, uint8x16_t, uint8x16_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vaddq_s16)))
int16x8_t __arm_vaddq_s16(int16x8_t, int16x8_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vaddq_s16)))
int16x8_t __arm_vaddq(int16x8_t, int16x8_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vaddq_s32)))
int32x4_t __arm_vaddq_s32(int32x4_t, int32x4_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vaddq_s32)))
int32x4_t __arm_vaddq(int32x4_t, int32x4_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vaddq_s8)))
int8x16_t __arm_vaddq_s8(int8x16_t, int8x16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vaddq_s8)))
int8x16_t __arm_vaddq(int8x16_t, int8x16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vaddq_u16)))
uint16x8_t __arm_vaddq_u16(uint16x8_t, uint16x8_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vaddq_u16)))
uint16x8_t __arm_vaddq(uint16x8_t, uint16x8_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vaddq_u32)))
uint32x4_t __arm_vaddq_u32(uint32x4_t, uint32x4_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vaddq_u32)))
uint32x4_t __arm_vaddq(uint32x4_t, uint32x4_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vaddq_u8)))
uint8x16_t __arm_vaddq_u8(uint8x16_t, uint8x16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vaddq_u8)))
uint8x16_t __arm_vaddq(uint8x16_t, uint8x16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vandq_m_s16)))
int16x8_t __arm_vandq_m_s16(int16x8_t, int16x8_t, int16x8_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vandq_m_s16)))
int16x8_t __arm_vandq_m(int16x8_t, int16x8_t, int16x8_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vandq_m_s32)))
int32x4_t __arm_vandq_m_s32(int32x4_t, int32x4_t, int32x4_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vandq_m_s32)))
int32x4_t __arm_vandq_m(int32x4_t, int32x4_t, int32x4_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vandq_m_s8)))
int8x16_t __arm_vandq_m_s8(int8x16_t, int8x16_t, int8x16_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vandq_m_s8)))
int8x16_t __arm_vandq_m(int8x16_t, int8x16_t, int8x16_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vandq_m_u16)))
uint16x8_t __arm_vandq_m_u16(uint16x8_t, uint16x8_t, uint16x8_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vandq_m_u16)))
uint16x8_t __arm_vandq_m(uint16x8_t, uint16x8_t, uint16x8_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vandq_m_u32)))
uint32x4_t __arm_vandq_m_u32(uint32x4_t, uint32x4_t, uint32x4_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vandq_m_u32)))
uint32x4_t __arm_vandq_m(uint32x4_t, uint32x4_t, uint32x4_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vandq_m_u8)))
uint8x16_t __arm_vandq_m_u8(uint8x16_t, uint8x16_t, uint8x16_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vandq_m_u8)))
uint8x16_t __arm_vandq_m(uint8x16_t, uint8x16_t, uint8x16_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vandq_s16)))
int16x8_t __arm_vandq_s16(int16x8_t, int16x8_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vandq_s16)))
int16x8_t __arm_vandq(int16x8_t, int16x8_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vandq_s32)))
int32x4_t __arm_vandq_s32(int32x4_t, int32x4_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vandq_s32)))
int32x4_t __arm_vandq(int32x4_t, int32x4_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vandq_s8)))
int8x16_t __arm_vandq_s8(int8x16_t, int8x16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vandq_s8)))
int8x16_t __arm_vandq(int8x16_t, int8x16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vandq_u16)))
uint16x8_t __arm_vandq_u16(uint16x8_t, uint16x8_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vandq_u16)))
uint16x8_t __arm_vandq(uint16x8_t, uint16x8_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vandq_u32)))
uint32x4_t __arm_vandq_u32(uint32x4_t, uint32x4_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vandq_u32)))
uint32x4_t __arm_vandq(uint32x4_t, uint32x4_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vandq_u8)))
uint8x16_t __arm_vandq_u8(uint8x16_t, uint8x16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vandq_u8)))
uint8x16_t __arm_vandq(uint8x16_t, uint8x16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vbicq_m_s16)))
int16x8_t __arm_vbicq_m_s16(int16x8_t, int16x8_t, int16x8_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vbicq_m_s16)))
int16x8_t __arm_vbicq_m(int16x8_t, int16x8_t, int16x8_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vbicq_m_s32)))
int32x4_t __arm_vbicq_m_s32(int32x4_t, int32x4_t, int32x4_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vbicq_m_s32)))
int32x4_t __arm_vbicq_m(int32x4_t, int32x4_t, int32x4_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vbicq_m_s8)))
int8x16_t __arm_vbicq_m_s8(int8x16_t, int8x16_t, int8x16_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vbicq_m_s8)))
int8x16_t __arm_vbicq_m(int8x16_t, int8x16_t, int8x16_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vbicq_m_u16)))
uint16x8_t __arm_vbicq_m_u16(uint16x8_t, uint16x8_t, uint16x8_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vbicq_m_u16)))
uint16x8_t __arm_vbicq_m(uint16x8_t, uint16x8_t, uint16x8_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vbicq_m_u32)))
uint32x4_t __arm_vbicq_m_u32(uint32x4_t, uint32x4_t, uint32x4_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vbicq_m_u32)))
uint32x4_t __arm_vbicq_m(uint32x4_t, uint32x4_t, uint32x4_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vbicq_m_u8)))
uint8x16_t __arm_vbicq_m_u8(uint8x16_t, uint8x16_t, uint8x16_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vbicq_m_u8)))
uint8x16_t __arm_vbicq_m(uint8x16_t, uint8x16_t, uint8x16_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vbicq_s16)))
int16x8_t __arm_vbicq_s16(int16x8_t, int16x8_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vbicq_s16)))
int16x8_t __arm_vbicq(int16x8_t, int16x8_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vbicq_s32)))
int32x4_t __arm_vbicq_s32(int32x4_t, int32x4_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vbicq_s32)))
int32x4_t __arm_vbicq(int32x4_t, int32x4_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vbicq_s8)))
int8x16_t __arm_vbicq_s8(int8x16_t, int8x16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vbicq_s8)))
int8x16_t __arm_vbicq(int8x16_t, int8x16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vbicq_u16)))
uint16x8_t __arm_vbicq_u16(uint16x8_t, uint16x8_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vbicq_u16)))
uint16x8_t __arm_vbicq(uint16x8_t, uint16x8_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vbicq_u32)))
uint32x4_t __arm_vbicq_u32(uint32x4_t, uint32x4_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vbicq_u32)))
uint32x4_t __arm_vbicq(uint32x4_t, uint32x4_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vbicq_u8)))
uint8x16_t __arm_vbicq_u8(uint8x16_t, uint8x16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vbicq_u8)))
uint8x16_t __arm_vbicq(uint8x16_t, uint8x16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpcsq_m_n_u16)))
mve_pred16_t __arm_vcmpcsq_m_n_u16(uint16x8_t, uint16_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpcsq_m_n_u16)))
mve_pred16_t __arm_vcmpcsq_m(uint16x8_t, uint16_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpcsq_m_n_u32)))
mve_pred16_t __arm_vcmpcsq_m_n_u32(uint32x4_t, uint32_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpcsq_m_n_u32)))
mve_pred16_t __arm_vcmpcsq_m(uint32x4_t, uint32_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpcsq_m_n_u8)))
mve_pred16_t __arm_vcmpcsq_m_n_u8(uint8x16_t, uint8_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpcsq_m_n_u8)))
mve_pred16_t __arm_vcmpcsq_m(uint8x16_t, uint8_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpcsq_m_u16)))
mve_pred16_t __arm_vcmpcsq_m_u16(uint16x8_t, uint16x8_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpcsq_m_u16)))
mve_pred16_t __arm_vcmpcsq_m(uint16x8_t, uint16x8_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpcsq_m_u32)))
mve_pred16_t __arm_vcmpcsq_m_u32(uint32x4_t, uint32x4_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpcsq_m_u32)))
mve_pred16_t __arm_vcmpcsq_m(uint32x4_t, uint32x4_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpcsq_m_u8)))
mve_pred16_t __arm_vcmpcsq_m_u8(uint8x16_t, uint8x16_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpcsq_m_u8)))
mve_pred16_t __arm_vcmpcsq_m(uint8x16_t, uint8x16_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpcsq_n_u16)))
mve_pred16_t __arm_vcmpcsq_n_u16(uint16x8_t, uint16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpcsq_n_u16)))
mve_pred16_t __arm_vcmpcsq(uint16x8_t, uint16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpcsq_n_u32)))
mve_pred16_t __arm_vcmpcsq_n_u32(uint32x4_t, uint32_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpcsq_n_u32)))
mve_pred16_t __arm_vcmpcsq(uint32x4_t, uint32_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpcsq_n_u8)))
mve_pred16_t __arm_vcmpcsq_n_u8(uint8x16_t, uint8_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpcsq_n_u8)))
mve_pred16_t __arm_vcmpcsq(uint8x16_t, uint8_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpcsq_u16)))
mve_pred16_t __arm_vcmpcsq_u16(uint16x8_t, uint16x8_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpcsq_u16)))
mve_pred16_t __arm_vcmpcsq(uint16x8_t, uint16x8_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpcsq_u32)))
mve_pred16_t __arm_vcmpcsq_u32(uint32x4_t, uint32x4_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpcsq_u32)))
mve_pred16_t __arm_vcmpcsq(uint32x4_t, uint32x4_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpcsq_u8)))
mve_pred16_t __arm_vcmpcsq_u8(uint8x16_t, uint8x16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpcsq_u8)))
mve_pred16_t __arm_vcmpcsq(uint8x16_t, uint8x16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpeqq_m_n_s16)))
mve_pred16_t __arm_vcmpeqq_m_n_s16(int16x8_t, int16_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpeqq_m_n_s16)))
mve_pred16_t __arm_vcmpeqq_m(int16x8_t, int16_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpeqq_m_n_s32)))
mve_pred16_t __arm_vcmpeqq_m_n_s32(int32x4_t, int32_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpeqq_m_n_s32)))
mve_pred16_t __arm_vcmpeqq_m(int32x4_t, int32_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpeqq_m_n_s8)))
mve_pred16_t __arm_vcmpeqq_m_n_s8(int8x16_t, int8_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpeqq_m_n_s8)))
mve_pred16_t __arm_vcmpeqq_m(int8x16_t, int8_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpeqq_m_n_u16)))
mve_pred16_t __arm_vcmpeqq_m_n_u16(uint16x8_t, uint16_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpeqq_m_n_u16)))
mve_pred16_t __arm_vcmpeqq_m(uint16x8_t, uint16_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpeqq_m_n_u32)))
mve_pred16_t __arm_vcmpeqq_m_n_u32(uint32x4_t, uint32_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpeqq_m_n_u32)))
mve_pred16_t __arm_vcmpeqq_m(uint32x4_t, uint32_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpeqq_m_n_u8)))
mve_pred16_t __arm_vcmpeqq_m_n_u8(uint8x16_t, uint8_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpeqq_m_n_u8)))
mve_pred16_t __arm_vcmpeqq_m(uint8x16_t, uint8_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpeqq_m_s16)))
mve_pred16_t __arm_vcmpeqq_m_s16(int16x8_t, int16x8_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpeqq_m_s16)))
mve_pred16_t __arm_vcmpeqq_m(int16x8_t, int16x8_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpeqq_m_s32)))
mve_pred16_t __arm_vcmpeqq_m_s32(int32x4_t, int32x4_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpeqq_m_s32)))
mve_pred16_t __arm_vcmpeqq_m(int32x4_t, int32x4_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpeqq_m_s8)))
mve_pred16_t __arm_vcmpeqq_m_s8(int8x16_t, int8x16_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpeqq_m_s8)))
mve_pred16_t __arm_vcmpeqq_m(int8x16_t, int8x16_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpeqq_m_u16)))
mve_pred16_t __arm_vcmpeqq_m_u16(uint16x8_t, uint16x8_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpeqq_m_u16)))
mve_pred16_t __arm_vcmpeqq_m(uint16x8_t, uint16x8_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpeqq_m_u32)))
mve_pred16_t __arm_vcmpeqq_m_u32(uint32x4_t, uint32x4_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpeqq_m_u32)))
mve_pred16_t __arm_vcmpeqq_m(uint32x4_t, uint32x4_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpeqq_m_u8)))
mve_pred16_t __arm_vcmpeqq_m_u8(uint8x16_t, uint8x16_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpeqq_m_u8)))
mve_pred16_t __arm_vcmpeqq_m(uint8x16_t, uint8x16_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpeqq_n_s16)))
mve_pred16_t __arm_vcmpeqq_n_s16(int16x8_t, int16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpeqq_n_s16)))
mve_pred16_t __arm_vcmpeqq(int16x8_t, int16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpeqq_n_s32)))
mve_pred16_t __arm_vcmpeqq_n_s32(int32x4_t, int32_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpeqq_n_s32)))
mve_pred16_t __arm_vcmpeqq(int32x4_t, int32_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpeqq_n_s8)))
mve_pred16_t __arm_vcmpeqq_n_s8(int8x16_t, int8_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpeqq_n_s8)))
mve_pred16_t __arm_vcmpeqq(int8x16_t, int8_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpeqq_n_u16)))
mve_pred16_t __arm_vcmpeqq_n_u16(uint16x8_t, uint16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpeqq_n_u16)))
mve_pred16_t __arm_vcmpeqq(uint16x8_t, uint16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpeqq_n_u32)))
mve_pred16_t __arm_vcmpeqq_n_u32(uint32x4_t, uint32_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpeqq_n_u32)))
mve_pred16_t __arm_vcmpeqq(uint32x4_t, uint32_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpeqq_n_u8)))
mve_pred16_t __arm_vcmpeqq_n_u8(uint8x16_t, uint8_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpeqq_n_u8)))
mve_pred16_t __arm_vcmpeqq(uint8x16_t, uint8_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpeqq_s16)))
mve_pred16_t __arm_vcmpeqq_s16(int16x8_t, int16x8_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpeqq_s16)))
mve_pred16_t __arm_vcmpeqq(int16x8_t, int16x8_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpeqq_s32)))
mve_pred16_t __arm_vcmpeqq_s32(int32x4_t, int32x4_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpeqq_s32)))
mve_pred16_t __arm_vcmpeqq(int32x4_t, int32x4_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpeqq_s8)))
mve_pred16_t __arm_vcmpeqq_s8(int8x16_t, int8x16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpeqq_s8)))
mve_pred16_t __arm_vcmpeqq(int8x16_t, int8x16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpeqq_u16)))
mve_pred16_t __arm_vcmpeqq_u16(uint16x8_t, uint16x8_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpeqq_u16)))
mve_pred16_t __arm_vcmpeqq(uint16x8_t, uint16x8_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpeqq_u32)))
mve_pred16_t __arm_vcmpeqq_u32(uint32x4_t, uint32x4_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpeqq_u32)))
mve_pred16_t __arm_vcmpeqq(uint32x4_t, uint32x4_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpeqq_u8)))
mve_pred16_t __arm_vcmpeqq_u8(uint8x16_t, uint8x16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpeqq_u8)))
mve_pred16_t __arm_vcmpeqq(uint8x16_t, uint8x16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpgeq_m_n_s16)))
mve_pred16_t __arm_vcmpgeq_m_n_s16(int16x8_t, int16_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpgeq_m_n_s16)))
mve_pred16_t __arm_vcmpgeq_m(int16x8_t, int16_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpgeq_m_n_s32)))
mve_pred16_t __arm_vcmpgeq_m_n_s32(int32x4_t, int32_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpgeq_m_n_s32)))
mve_pred16_t __arm_vcmpgeq_m(int32x4_t, int32_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpgeq_m_n_s8)))
mve_pred16_t __arm_vcmpgeq_m_n_s8(int8x16_t, int8_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpgeq_m_n_s8)))
mve_pred16_t __arm_vcmpgeq_m(int8x16_t, int8_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpgeq_m_s16)))
mve_pred16_t __arm_vcmpgeq_m_s16(int16x8_t, int16x8_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpgeq_m_s16)))
mve_pred16_t __arm_vcmpgeq_m(int16x8_t, int16x8_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpgeq_m_s32)))
mve_pred16_t __arm_vcmpgeq_m_s32(int32x4_t, int32x4_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpgeq_m_s32)))
mve_pred16_t __arm_vcmpgeq_m(int32x4_t, int32x4_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpgeq_m_s8)))
mve_pred16_t __arm_vcmpgeq_m_s8(int8x16_t, int8x16_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpgeq_m_s8)))
mve_pred16_t __arm_vcmpgeq_m(int8x16_t, int8x16_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpgeq_n_s16)))
mve_pred16_t __arm_vcmpgeq_n_s16(int16x8_t, int16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpgeq_n_s16)))
mve_pred16_t __arm_vcmpgeq(int16x8_t, int16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpgeq_n_s32)))
mve_pred16_t __arm_vcmpgeq_n_s32(int32x4_t, int32_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpgeq_n_s32)))
mve_pred16_t __arm_vcmpgeq(int32x4_t, int32_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpgeq_n_s8)))
mve_pred16_t __arm_vcmpgeq_n_s8(int8x16_t, int8_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpgeq_n_s8)))
mve_pred16_t __arm_vcmpgeq(int8x16_t, int8_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpgeq_s16)))
mve_pred16_t __arm_vcmpgeq_s16(int16x8_t, int16x8_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpgeq_s16)))
mve_pred16_t __arm_vcmpgeq(int16x8_t, int16x8_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpgeq_s32)))
mve_pred16_t __arm_vcmpgeq_s32(int32x4_t, int32x4_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpgeq_s32)))
mve_pred16_t __arm_vcmpgeq(int32x4_t, int32x4_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpgeq_s8)))
mve_pred16_t __arm_vcmpgeq_s8(int8x16_t, int8x16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpgeq_s8)))
mve_pred16_t __arm_vcmpgeq(int8x16_t, int8x16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpgtq_m_n_s16)))
mve_pred16_t __arm_vcmpgtq_m_n_s16(int16x8_t, int16_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpgtq_m_n_s16)))
mve_pred16_t __arm_vcmpgtq_m(int16x8_t, int16_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpgtq_m_n_s32)))
mve_pred16_t __arm_vcmpgtq_m_n_s32(int32x4_t, int32_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpgtq_m_n_s32)))
mve_pred16_t __arm_vcmpgtq_m(int32x4_t, int32_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpgtq_m_n_s8)))
mve_pred16_t __arm_vcmpgtq_m_n_s8(int8x16_t, int8_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpgtq_m_n_s8)))
mve_pred16_t __arm_vcmpgtq_m(int8x16_t, int8_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpgtq_m_s16)))
mve_pred16_t __arm_vcmpgtq_m_s16(int16x8_t, int16x8_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpgtq_m_s16)))
mve_pred16_t __arm_vcmpgtq_m(int16x8_t, int16x8_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpgtq_m_s32)))
mve_pred16_t __arm_vcmpgtq_m_s32(int32x4_t, int32x4_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpgtq_m_s32)))
mve_pred16_t __arm_vcmpgtq_m(int32x4_t, int32x4_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpgtq_m_s8)))
mve_pred16_t __arm_vcmpgtq_m_s8(int8x16_t, int8x16_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpgtq_m_s8)))
mve_pred16_t __arm_vcmpgtq_m(int8x16_t, int8x16_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpgtq_n_s16)))
mve_pred16_t __arm_vcmpgtq_n_s16(int16x8_t, int16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpgtq_n_s16)))
mve_pred16_t __arm_vcmpgtq(int16x8_t, int16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpgtq_n_s32)))
mve_pred16_t __arm_vcmpgtq_n_s32(int32x4_t, int32_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpgtq_n_s32)))
mve_pred16_t __arm_vcmpgtq(int32x4_t, int32_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpgtq_n_s8)))
mve_pred16_t __arm_vcmpgtq_n_s8(int8x16_t, int8_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpgtq_n_s8)))
mve_pred16_t __arm_vcmpgtq(int8x16_t, int8_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpgtq_s16)))
mve_pred16_t __arm_vcmpgtq_s16(int16x8_t, int16x8_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpgtq_s16)))
mve_pred16_t __arm_vcmpgtq(int16x8_t, int16x8_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpgtq_s32)))
mve_pred16_t __arm_vcmpgtq_s32(int32x4_t, int32x4_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpgtq_s32)))
mve_pred16_t __arm_vcmpgtq(int32x4_t, int32x4_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpgtq_s8)))
mve_pred16_t __arm_vcmpgtq_s8(int8x16_t, int8x16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpgtq_s8)))
mve_pred16_t __arm_vcmpgtq(int8x16_t, int8x16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmphiq_m_n_u16)))
mve_pred16_t __arm_vcmphiq_m_n_u16(uint16x8_t, uint16_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmphiq_m_n_u16)))
mve_pred16_t __arm_vcmphiq_m(uint16x8_t, uint16_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmphiq_m_n_u32)))
mve_pred16_t __arm_vcmphiq_m_n_u32(uint32x4_t, uint32_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmphiq_m_n_u32)))
mve_pred16_t __arm_vcmphiq_m(uint32x4_t, uint32_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmphiq_m_n_u8)))
mve_pred16_t __arm_vcmphiq_m_n_u8(uint8x16_t, uint8_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmphiq_m_n_u8)))
mve_pred16_t __arm_vcmphiq_m(uint8x16_t, uint8_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmphiq_m_u16)))
mve_pred16_t __arm_vcmphiq_m_u16(uint16x8_t, uint16x8_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmphiq_m_u16)))
mve_pred16_t __arm_vcmphiq_m(uint16x8_t, uint16x8_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmphiq_m_u32)))
mve_pred16_t __arm_vcmphiq_m_u32(uint32x4_t, uint32x4_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmphiq_m_u32)))
mve_pred16_t __arm_vcmphiq_m(uint32x4_t, uint32x4_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmphiq_m_u8)))
mve_pred16_t __arm_vcmphiq_m_u8(uint8x16_t, uint8x16_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmphiq_m_u8)))
mve_pred16_t __arm_vcmphiq_m(uint8x16_t, uint8x16_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmphiq_n_u16)))
mve_pred16_t __arm_vcmphiq_n_u16(uint16x8_t, uint16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmphiq_n_u16)))
mve_pred16_t __arm_vcmphiq(uint16x8_t, uint16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmphiq_n_u32)))
mve_pred16_t __arm_vcmphiq_n_u32(uint32x4_t, uint32_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmphiq_n_u32)))
mve_pred16_t __arm_vcmphiq(uint32x4_t, uint32_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmphiq_n_u8)))
mve_pred16_t __arm_vcmphiq_n_u8(uint8x16_t, uint8_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmphiq_n_u8)))
mve_pred16_t __arm_vcmphiq(uint8x16_t, uint8_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmphiq_u16)))
mve_pred16_t __arm_vcmphiq_u16(uint16x8_t, uint16x8_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmphiq_u16)))
mve_pred16_t __arm_vcmphiq(uint16x8_t, uint16x8_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmphiq_u32)))
mve_pred16_t __arm_vcmphiq_u32(uint32x4_t, uint32x4_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmphiq_u32)))
mve_pred16_t __arm_vcmphiq(uint32x4_t, uint32x4_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmphiq_u8)))
mve_pred16_t __arm_vcmphiq_u8(uint8x16_t, uint8x16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmphiq_u8)))
mve_pred16_t __arm_vcmphiq(uint8x16_t, uint8x16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpleq_m_n_s16)))
mve_pred16_t __arm_vcmpleq_m_n_s16(int16x8_t, int16_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpleq_m_n_s16)))
mve_pred16_t __arm_vcmpleq_m(int16x8_t, int16_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpleq_m_n_s32)))
mve_pred16_t __arm_vcmpleq_m_n_s32(int32x4_t, int32_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpleq_m_n_s32)))
mve_pred16_t __arm_vcmpleq_m(int32x4_t, int32_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpleq_m_n_s8)))
mve_pred16_t __arm_vcmpleq_m_n_s8(int8x16_t, int8_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpleq_m_n_s8)))
mve_pred16_t __arm_vcmpleq_m(int8x16_t, int8_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpleq_m_s16)))
mve_pred16_t __arm_vcmpleq_m_s16(int16x8_t, int16x8_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpleq_m_s16)))
mve_pred16_t __arm_vcmpleq_m(int16x8_t, int16x8_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpleq_m_s32)))
mve_pred16_t __arm_vcmpleq_m_s32(int32x4_t, int32x4_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpleq_m_s32)))
mve_pred16_t __arm_vcmpleq_m(int32x4_t, int32x4_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpleq_m_s8)))
mve_pred16_t __arm_vcmpleq_m_s8(int8x16_t, int8x16_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpleq_m_s8)))
mve_pred16_t __arm_vcmpleq_m(int8x16_t, int8x16_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpleq_n_s16)))
mve_pred16_t __arm_vcmpleq_n_s16(int16x8_t, int16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpleq_n_s16)))
mve_pred16_t __arm_vcmpleq(int16x8_t, int16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpleq_n_s32)))
mve_pred16_t __arm_vcmpleq_n_s32(int32x4_t, int32_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpleq_n_s32)))
mve_pred16_t __arm_vcmpleq(int32x4_t, int32_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpleq_n_s8)))
mve_pred16_t __arm_vcmpleq_n_s8(int8x16_t, int8_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpleq_n_s8)))
mve_pred16_t __arm_vcmpleq(int8x16_t, int8_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpleq_s16)))
mve_pred16_t __arm_vcmpleq_s16(int16x8_t, int16x8_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpleq_s16)))
mve_pred16_t __arm_vcmpleq(int16x8_t, int16x8_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpleq_s32)))
mve_pred16_t __arm_vcmpleq_s32(int32x4_t, int32x4_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpleq_s32)))
mve_pred16_t __arm_vcmpleq(int32x4_t, int32x4_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpleq_s8)))
mve_pred16_t __arm_vcmpleq_s8(int8x16_t, int8x16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpleq_s8)))
mve_pred16_t __arm_vcmpleq(int8x16_t, int8x16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpltq_m_n_s16)))
mve_pred16_t __arm_vcmpltq_m_n_s16(int16x8_t, int16_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpltq_m_n_s16)))
mve_pred16_t __arm_vcmpltq_m(int16x8_t, int16_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpltq_m_n_s32)))
mve_pred16_t __arm_vcmpltq_m_n_s32(int32x4_t, int32_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpltq_m_n_s32)))
mve_pred16_t __arm_vcmpltq_m(int32x4_t, int32_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpltq_m_n_s8)))
mve_pred16_t __arm_vcmpltq_m_n_s8(int8x16_t, int8_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpltq_m_n_s8)))
mve_pred16_t __arm_vcmpltq_m(int8x16_t, int8_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpltq_m_s16)))
mve_pred16_t __arm_vcmpltq_m_s16(int16x8_t, int16x8_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpltq_m_s16)))
mve_pred16_t __arm_vcmpltq_m(int16x8_t, int16x8_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpltq_m_s32)))
mve_pred16_t __arm_vcmpltq_m_s32(int32x4_t, int32x4_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpltq_m_s32)))
mve_pred16_t __arm_vcmpltq_m(int32x4_t, int32x4_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpltq_m_s8)))
mve_pred16_t __arm_vcmpltq_m_s8(int8x16_t, int8x16_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpltq_m_s8)))
mve_pred16_t __arm_vcmpltq_m(int8x16_t, int8x16_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpltq_n_s16)))
mve_pred16_t __arm_vcmpltq_n_s16(int16x8_t, int16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpltq_n_s16)))
mve_pred16_t __arm_vcmpltq(int16x8_t, int16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpltq_n_s32)))
mve_pred16_t __arm_vcmpltq_n_s32(int32x4_t, int32_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpltq_n_s32)))
mve_pred16_t __arm_vcmpltq(int32x4_t, int32_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpltq_n_s8)))
mve_pred16_t __arm_vcmpltq_n_s8(int8x16_t, int8_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpltq_n_s8)))
mve_pred16_t __arm_vcmpltq(int8x16_t, int8_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpltq_s16)))
mve_pred16_t __arm_vcmpltq_s16(int16x8_t, int16x8_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpltq_s16)))
mve_pred16_t __arm_vcmpltq(int16x8_t, int16x8_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpltq_s32)))
mve_pred16_t __arm_vcmpltq_s32(int32x4_t, int32x4_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpltq_s32)))
mve_pred16_t __arm_vcmpltq(int32x4_t, int32x4_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpltq_s8)))
mve_pred16_t __arm_vcmpltq_s8(int8x16_t, int8x16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpltq_s8)))
mve_pred16_t __arm_vcmpltq(int8x16_t, int8x16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpneq_m_n_s16)))
mve_pred16_t __arm_vcmpneq_m_n_s16(int16x8_t, int16_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpneq_m_n_s16)))
mve_pred16_t __arm_vcmpneq_m(int16x8_t, int16_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpneq_m_n_s32)))
mve_pred16_t __arm_vcmpneq_m_n_s32(int32x4_t, int32_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpneq_m_n_s32)))
mve_pred16_t __arm_vcmpneq_m(int32x4_t, int32_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpneq_m_n_s8)))
mve_pred16_t __arm_vcmpneq_m_n_s8(int8x16_t, int8_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpneq_m_n_s8)))
mve_pred16_t __arm_vcmpneq_m(int8x16_t, int8_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpneq_m_n_u16)))
mve_pred16_t __arm_vcmpneq_m_n_u16(uint16x8_t, uint16_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpneq_m_n_u16)))
mve_pred16_t __arm_vcmpneq_m(uint16x8_t, uint16_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpneq_m_n_u32)))
mve_pred16_t __arm_vcmpneq_m_n_u32(uint32x4_t, uint32_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpneq_m_n_u32)))
mve_pred16_t __arm_vcmpneq_m(uint32x4_t, uint32_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpneq_m_n_u8)))
mve_pred16_t __arm_vcmpneq_m_n_u8(uint8x16_t, uint8_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpneq_m_n_u8)))
mve_pred16_t __arm_vcmpneq_m(uint8x16_t, uint8_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpneq_m_s16)))
mve_pred16_t __arm_vcmpneq_m_s16(int16x8_t, int16x8_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpneq_m_s16)))
mve_pred16_t __arm_vcmpneq_m(int16x8_t, int16x8_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpneq_m_s32)))
mve_pred16_t __arm_vcmpneq_m_s32(int32x4_t, int32x4_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpneq_m_s32)))
mve_pred16_t __arm_vcmpneq_m(int32x4_t, int32x4_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpneq_m_s8)))
mve_pred16_t __arm_vcmpneq_m_s8(int8x16_t, int8x16_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpneq_m_s8)))
mve_pred16_t __arm_vcmpneq_m(int8x16_t, int8x16_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpneq_m_u16)))
mve_pred16_t __arm_vcmpneq_m_u16(uint16x8_t, uint16x8_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpneq_m_u16)))
mve_pred16_t __arm_vcmpneq_m(uint16x8_t, uint16x8_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpneq_m_u32)))
mve_pred16_t __arm_vcmpneq_m_u32(uint32x4_t, uint32x4_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpneq_m_u32)))
mve_pred16_t __arm_vcmpneq_m(uint32x4_t, uint32x4_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpneq_m_u8)))
mve_pred16_t __arm_vcmpneq_m_u8(uint8x16_t, uint8x16_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpneq_m_u8)))
mve_pred16_t __arm_vcmpneq_m(uint8x16_t, uint8x16_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpneq_n_s16)))
mve_pred16_t __arm_vcmpneq_n_s16(int16x8_t, int16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpneq_n_s16)))
mve_pred16_t __arm_vcmpneq(int16x8_t, int16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpneq_n_s32)))
mve_pred16_t __arm_vcmpneq_n_s32(int32x4_t, int32_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpneq_n_s32)))
mve_pred16_t __arm_vcmpneq(int32x4_t, int32_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpneq_n_s8)))
mve_pred16_t __arm_vcmpneq_n_s8(int8x16_t, int8_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpneq_n_s8)))
mve_pred16_t __arm_vcmpneq(int8x16_t, int8_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpneq_n_u16)))
mve_pred16_t __arm_vcmpneq_n_u16(uint16x8_t, uint16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpneq_n_u16)))
mve_pred16_t __arm_vcmpneq(uint16x8_t, uint16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpneq_n_u32)))
mve_pred16_t __arm_vcmpneq_n_u32(uint32x4_t, uint32_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpneq_n_u32)))
mve_pred16_t __arm_vcmpneq(uint32x4_t, uint32_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpneq_n_u8)))
mve_pred16_t __arm_vcmpneq_n_u8(uint8x16_t, uint8_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpneq_n_u8)))
mve_pred16_t __arm_vcmpneq(uint8x16_t, uint8_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpneq_s16)))
mve_pred16_t __arm_vcmpneq_s16(int16x8_t, int16x8_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpneq_s16)))
mve_pred16_t __arm_vcmpneq(int16x8_t, int16x8_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpneq_s32)))
mve_pred16_t __arm_vcmpneq_s32(int32x4_t, int32x4_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpneq_s32)))
mve_pred16_t __arm_vcmpneq(int32x4_t, int32x4_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpneq_s8)))
mve_pred16_t __arm_vcmpneq_s8(int8x16_t, int8x16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpneq_s8)))
mve_pred16_t __arm_vcmpneq(int8x16_t, int8x16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpneq_u16)))
mve_pred16_t __arm_vcmpneq_u16(uint16x8_t, uint16x8_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpneq_u16)))
mve_pred16_t __arm_vcmpneq(uint16x8_t, uint16x8_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpneq_u32)))
mve_pred16_t __arm_vcmpneq_u32(uint32x4_t, uint32x4_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpneq_u32)))
mve_pred16_t __arm_vcmpneq(uint32x4_t, uint32x4_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpneq_u8)))
mve_pred16_t __arm_vcmpneq_u8(uint8x16_t, uint8x16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpneq_u8)))
mve_pred16_t __arm_vcmpneq(uint8x16_t, uint8x16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcreateq_s16)))
int16x8_t __arm_vcreateq_s16(uint64_t, uint64_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcreateq_s32)))
int32x4_t __arm_vcreateq_s32(uint64_t, uint64_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcreateq_s64)))
int64x2_t __arm_vcreateq_s64(uint64_t, uint64_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcreateq_s8)))
int8x16_t __arm_vcreateq_s8(uint64_t, uint64_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcreateq_u16)))
uint16x8_t __arm_vcreateq_u16(uint64_t, uint64_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcreateq_u32)))
uint32x4_t __arm_vcreateq_u32(uint64_t, uint64_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcreateq_u64)))
uint64x2_t __arm_vcreateq_u64(uint64_t, uint64_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcreateq_u8)))
uint8x16_t __arm_vcreateq_u8(uint64_t, uint64_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_veorq_m_s16)))
int16x8_t __arm_veorq_m_s16(int16x8_t, int16x8_t, int16x8_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_veorq_m_s16)))
int16x8_t __arm_veorq_m(int16x8_t, int16x8_t, int16x8_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_veorq_m_s32)))
int32x4_t __arm_veorq_m_s32(int32x4_t, int32x4_t, int32x4_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_veorq_m_s32)))
int32x4_t __arm_veorq_m(int32x4_t, int32x4_t, int32x4_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_veorq_m_s8)))
int8x16_t __arm_veorq_m_s8(int8x16_t, int8x16_t, int8x16_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_veorq_m_s8)))
int8x16_t __arm_veorq_m(int8x16_t, int8x16_t, int8x16_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_veorq_m_u16)))
uint16x8_t __arm_veorq_m_u16(uint16x8_t, uint16x8_t, uint16x8_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_veorq_m_u16)))
uint16x8_t __arm_veorq_m(uint16x8_t, uint16x8_t, uint16x8_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_veorq_m_u32)))
uint32x4_t __arm_veorq_m_u32(uint32x4_t, uint32x4_t, uint32x4_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_veorq_m_u32)))
uint32x4_t __arm_veorq_m(uint32x4_t, uint32x4_t, uint32x4_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_veorq_m_u8)))
uint8x16_t __arm_veorq_m_u8(uint8x16_t, uint8x16_t, uint8x16_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_veorq_m_u8)))
uint8x16_t __arm_veorq_m(uint8x16_t, uint8x16_t, uint8x16_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_veorq_s16)))
int16x8_t __arm_veorq_s16(int16x8_t, int16x8_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_veorq_s16)))
int16x8_t __arm_veorq(int16x8_t, int16x8_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_veorq_s32)))
int32x4_t __arm_veorq_s32(int32x4_t, int32x4_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_veorq_s32)))
int32x4_t __arm_veorq(int32x4_t, int32x4_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_veorq_s8)))
int8x16_t __arm_veorq_s8(int8x16_t, int8x16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_veorq_s8)))
int8x16_t __arm_veorq(int8x16_t, int8x16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_veorq_u16)))
uint16x8_t __arm_veorq_u16(uint16x8_t, uint16x8_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_veorq_u16)))
uint16x8_t __arm_veorq(uint16x8_t, uint16x8_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_veorq_u32)))
uint32x4_t __arm_veorq_u32(uint32x4_t, uint32x4_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_veorq_u32)))
uint32x4_t __arm_veorq(uint32x4_t, uint32x4_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_veorq_u8)))
uint8x16_t __arm_veorq_u8(uint8x16_t, uint8x16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_veorq_u8)))
uint8x16_t __arm_veorq(uint8x16_t, uint8x16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vgetq_lane_s16)))
int16_t __arm_vgetq_lane_s16(int16x8_t, int);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vgetq_lane_s16)))
int16_t __arm_vgetq_lane(int16x8_t, int);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vgetq_lane_s32)))
int32_t __arm_vgetq_lane_s32(int32x4_t, int);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vgetq_lane_s32)))
int32_t __arm_vgetq_lane(int32x4_t, int);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vgetq_lane_s64)))
int64_t __arm_vgetq_lane_s64(int64x2_t, int);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vgetq_lane_s64)))
int64_t __arm_vgetq_lane(int64x2_t, int);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vgetq_lane_s8)))
int8_t __arm_vgetq_lane_s8(int8x16_t, int);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vgetq_lane_s8)))
int8_t __arm_vgetq_lane(int8x16_t, int);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vgetq_lane_u16)))
uint16_t __arm_vgetq_lane_u16(uint16x8_t, int);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vgetq_lane_u16)))
uint16_t __arm_vgetq_lane(uint16x8_t, int);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vgetq_lane_u32)))
uint32_t __arm_vgetq_lane_u32(uint32x4_t, int);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vgetq_lane_u32)))
uint32_t __arm_vgetq_lane(uint32x4_t, int);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vgetq_lane_u64)))
uint64_t __arm_vgetq_lane_u64(uint64x2_t, int);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vgetq_lane_u64)))
uint64_t __arm_vgetq_lane(uint64x2_t, int);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vgetq_lane_u8)))
uint8_t __arm_vgetq_lane_u8(uint8x16_t, int);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vgetq_lane_u8)))
uint8_t __arm_vgetq_lane(uint8x16_t, int);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vld1q_s16)))
int16x8_t __arm_vld1q_s16(const int16_t *);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vld1q_s16)))
int16x8_t __arm_vld1q(const int16_t *);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vld1q_s32)))
int32x4_t __arm_vld1q_s32(const int32_t *);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vld1q_s32)))
int32x4_t __arm_vld1q(const int32_t *);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vld1q_s8)))
int8x16_t __arm_vld1q_s8(const int8_t *);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vld1q_s8)))
int8x16_t __arm_vld1q(const int8_t *);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vld1q_u16)))
uint16x8_t __arm_vld1q_u16(const uint16_t *);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vld1q_u16)))
uint16x8_t __arm_vld1q(const uint16_t *);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vld1q_u32)))
uint32x4_t __arm_vld1q_u32(const uint32_t *);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vld1q_u32)))
uint32x4_t __arm_vld1q(const uint32_t *);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vld1q_u8)))
uint8x16_t __arm_vld1q_u8(const uint8_t *);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vld1q_u8)))
uint8x16_t __arm_vld1q(const uint8_t *);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vld1q_z_s16)))
int16x8_t __arm_vld1q_z_s16(const int16_t *, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vld1q_z_s16)))
int16x8_t __arm_vld1q_z(const int16_t *, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vld1q_z_s32)))
int32x4_t __arm_vld1q_z_s32(const int32_t *, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vld1q_z_s32)))
int32x4_t __arm_vld1q_z(const int32_t *, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vld1q_z_s8)))
int8x16_t __arm_vld1q_z_s8(const int8_t *, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vld1q_z_s8)))
int8x16_t __arm_vld1q_z(const int8_t *, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vld1q_z_u16)))
uint16x8_t __arm_vld1q_z_u16(const uint16_t *, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vld1q_z_u16)))
uint16x8_t __arm_vld1q_z(const uint16_t *, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vld1q_z_u32)))
uint32x4_t __arm_vld1q_z_u32(const uint32_t *, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vld1q_z_u32)))
uint32x4_t __arm_vld1q_z(const uint32_t *, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vld1q_z_u8)))
uint8x16_t __arm_vld1q_z_u8(const uint8_t *, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vld1q_z_u8)))
uint8x16_t __arm_vld1q_z(const uint8_t *, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vld2q_s16)))
int16x8x2_t __arm_vld2q_s16(const int16_t *);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vld2q_s16)))
int16x8x2_t __arm_vld2q(const int16_t *);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vld2q_s32)))
int32x4x2_t __arm_vld2q_s32(const int32_t *);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vld2q_s32)))
int32x4x2_t __arm_vld2q(const int32_t *);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vld2q_s8)))
int8x16x2_t __arm_vld2q_s8(const int8_t *);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vld2q_s8)))
int8x16x2_t __arm_vld2q(const int8_t *);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vld2q_u16)))
uint16x8x2_t __arm_vld2q_u16(const uint16_t *);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vld2q_u16)))
uint16x8x2_t __arm_vld2q(const uint16_t *);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vld2q_u32)))
uint32x4x2_t __arm_vld2q_u32(const uint32_t *);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vld2q_u32)))
uint32x4x2_t __arm_vld2q(const uint32_t *);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vld2q_u8)))
uint8x16x2_t __arm_vld2q_u8(const uint8_t *);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vld2q_u8)))
uint8x16x2_t __arm_vld2q(const uint8_t *);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vld4q_s16)))
int16x8x4_t __arm_vld4q_s16(const int16_t *);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vld4q_s16)))
int16x8x4_t __arm_vld4q(const int16_t *);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vld4q_s32)))
int32x4x4_t __arm_vld4q_s32(const int32_t *);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vld4q_s32)))
int32x4x4_t __arm_vld4q(const int32_t *);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vld4q_s8)))
int8x16x4_t __arm_vld4q_s8(const int8_t *);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vld4q_s8)))
int8x16x4_t __arm_vld4q(const int8_t *);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vld4q_u16)))
uint16x8x4_t __arm_vld4q_u16(const uint16_t *);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vld4q_u16)))
uint16x8x4_t __arm_vld4q(const uint16_t *);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vld4q_u32)))
uint32x4x4_t __arm_vld4q_u32(const uint32_t *);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vld4q_u32)))
uint32x4x4_t __arm_vld4q(const uint32_t *);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vld4q_u8)))
uint8x16x4_t __arm_vld4q_u8(const uint8_t *);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vld4q_u8)))
uint8x16x4_t __arm_vld4q(const uint8_t *);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vldrbq_gather_offset_s16)))
int16x8_t __arm_vldrbq_gather_offset_s16(const int8_t *, uint16x8_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vldrbq_gather_offset_s16)))
int16x8_t __arm_vldrbq_gather_offset(const int8_t *, uint16x8_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vldrbq_gather_offset_s32)))
int32x4_t __arm_vldrbq_gather_offset_s32(const int8_t *, uint32x4_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vldrbq_gather_offset_s32)))
int32x4_t __arm_vldrbq_gather_offset(const int8_t *, uint32x4_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vldrbq_gather_offset_s8)))
int8x16_t __arm_vldrbq_gather_offset_s8(const int8_t *, uint8x16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vldrbq_gather_offset_s8)))
int8x16_t __arm_vldrbq_gather_offset(const int8_t *, uint8x16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vldrbq_gather_offset_u16)))
uint16x8_t __arm_vldrbq_gather_offset_u16(const uint8_t *, uint16x8_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vldrbq_gather_offset_u16)))
uint16x8_t __arm_vldrbq_gather_offset(const uint8_t *, uint16x8_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vldrbq_gather_offset_u32)))
uint32x4_t __arm_vldrbq_gather_offset_u32(const uint8_t *, uint32x4_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vldrbq_gather_offset_u32)))
uint32x4_t __arm_vldrbq_gather_offset(const uint8_t *, uint32x4_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vldrbq_gather_offset_u8)))
uint8x16_t __arm_vldrbq_gather_offset_u8(const uint8_t *, uint8x16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vldrbq_gather_offset_u8)))
uint8x16_t __arm_vldrbq_gather_offset(const uint8_t *, uint8x16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vldrbq_gather_offset_z_s16)))
int16x8_t __arm_vldrbq_gather_offset_z_s16(const int8_t *, uint16x8_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vldrbq_gather_offset_z_s16)))
int16x8_t __arm_vldrbq_gather_offset_z(const int8_t *, uint16x8_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vldrbq_gather_offset_z_s32)))
int32x4_t __arm_vldrbq_gather_offset_z_s32(const int8_t *, uint32x4_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vldrbq_gather_offset_z_s32)))
int32x4_t __arm_vldrbq_gather_offset_z(const int8_t *, uint32x4_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vldrbq_gather_offset_z_s8)))
int8x16_t __arm_vldrbq_gather_offset_z_s8(const int8_t *, uint8x16_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vldrbq_gather_offset_z_s8)))
int8x16_t __arm_vldrbq_gather_offset_z(const int8_t *, uint8x16_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vldrbq_gather_offset_z_u16)))
uint16x8_t __arm_vldrbq_gather_offset_z_u16(const uint8_t *, uint16x8_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vldrbq_gather_offset_z_u16)))
uint16x8_t __arm_vldrbq_gather_offset_z(const uint8_t *, uint16x8_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vldrbq_gather_offset_z_u32)))
uint32x4_t __arm_vldrbq_gather_offset_z_u32(const uint8_t *, uint32x4_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vldrbq_gather_offset_z_u32)))
uint32x4_t __arm_vldrbq_gather_offset_z(const uint8_t *, uint32x4_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vldrbq_gather_offset_z_u8)))
uint8x16_t __arm_vldrbq_gather_offset_z_u8(const uint8_t *, uint8x16_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vldrbq_gather_offset_z_u8)))
uint8x16_t __arm_vldrbq_gather_offset_z(const uint8_t *, uint8x16_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vldrbq_s16)))
int16x8_t __arm_vldrbq_s16(const int8_t *);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vldrbq_s32)))
int32x4_t __arm_vldrbq_s32(const int8_t *);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vldrbq_s8)))
int8x16_t __arm_vldrbq_s8(const int8_t *);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vldrbq_u16)))
uint16x8_t __arm_vldrbq_u16(const uint8_t *);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vldrbq_u32)))
uint32x4_t __arm_vldrbq_u32(const uint8_t *);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vldrbq_u8)))
uint8x16_t __arm_vldrbq_u8(const uint8_t *);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vldrbq_z_s16)))
int16x8_t __arm_vldrbq_z_s16(const int8_t *, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vldrbq_z_s32)))
int32x4_t __arm_vldrbq_z_s32(const int8_t *, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vldrbq_z_s8)))
int8x16_t __arm_vldrbq_z_s8(const int8_t *, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vldrbq_z_u16)))
uint16x8_t __arm_vldrbq_z_u16(const uint8_t *, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vldrbq_z_u32)))
uint32x4_t __arm_vldrbq_z_u32(const uint8_t *, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vldrbq_z_u8)))
uint8x16_t __arm_vldrbq_z_u8(const uint8_t *, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vldrdq_gather_base_s64)))
int64x2_t __arm_vldrdq_gather_base_s64(uint64x2_t, int);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vldrdq_gather_base_u64)))
uint64x2_t __arm_vldrdq_gather_base_u64(uint64x2_t, int);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vldrdq_gather_base_wb_s64)))
int64x2_t __arm_vldrdq_gather_base_wb_s64(uint64x2_t *, int);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vldrdq_gather_base_wb_u64)))
uint64x2_t __arm_vldrdq_gather_base_wb_u64(uint64x2_t *, int);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vldrdq_gather_base_wb_z_s64)))
int64x2_t __arm_vldrdq_gather_base_wb_z_s64(uint64x2_t *, int, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vldrdq_gather_base_wb_z_u64)))
uint64x2_t __arm_vldrdq_gather_base_wb_z_u64(uint64x2_t *, int, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vldrdq_gather_base_z_s64)))
int64x2_t __arm_vldrdq_gather_base_z_s64(uint64x2_t, int, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vldrdq_gather_base_z_u64)))
uint64x2_t __arm_vldrdq_gather_base_z_u64(uint64x2_t, int, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vldrdq_gather_offset_s64)))
int64x2_t __arm_vldrdq_gather_offset_s64(const int64_t *, uint64x2_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vldrdq_gather_offset_s64)))
int64x2_t __arm_vldrdq_gather_offset(const int64_t *, uint64x2_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vldrdq_gather_offset_u64)))
uint64x2_t __arm_vldrdq_gather_offset_u64(const uint64_t *, uint64x2_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vldrdq_gather_offset_u64)))
uint64x2_t __arm_vldrdq_gather_offset(const uint64_t *, uint64x2_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vldrdq_gather_offset_z_s64)))
int64x2_t __arm_vldrdq_gather_offset_z_s64(const int64_t *, uint64x2_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vldrdq_gather_offset_z_s64)))
int64x2_t __arm_vldrdq_gather_offset_z(const int64_t *, uint64x2_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vldrdq_gather_offset_z_u64)))
uint64x2_t __arm_vldrdq_gather_offset_z_u64(const uint64_t *, uint64x2_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vldrdq_gather_offset_z_u64)))
uint64x2_t __arm_vldrdq_gather_offset_z(const uint64_t *, uint64x2_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vldrdq_gather_shifted_offset_s64)))
int64x2_t __arm_vldrdq_gather_shifted_offset_s64(const int64_t *, uint64x2_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vldrdq_gather_shifted_offset_s64)))
int64x2_t __arm_vldrdq_gather_shifted_offset(const int64_t *, uint64x2_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vldrdq_gather_shifted_offset_u64)))
uint64x2_t __arm_vldrdq_gather_shifted_offset_u64(const uint64_t *, uint64x2_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vldrdq_gather_shifted_offset_u64)))
uint64x2_t __arm_vldrdq_gather_shifted_offset(const uint64_t *, uint64x2_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vldrdq_gather_shifted_offset_z_s64)))
int64x2_t __arm_vldrdq_gather_shifted_offset_z_s64(const int64_t *, uint64x2_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vldrdq_gather_shifted_offset_z_s64)))
int64x2_t __arm_vldrdq_gather_shifted_offset_z(const int64_t *, uint64x2_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vldrdq_gather_shifted_offset_z_u64)))
uint64x2_t __arm_vldrdq_gather_shifted_offset_z_u64(const uint64_t *, uint64x2_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vldrdq_gather_shifted_offset_z_u64)))
uint64x2_t __arm_vldrdq_gather_shifted_offset_z(const uint64_t *, uint64x2_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vldrhq_gather_offset_s16)))
int16x8_t __arm_vldrhq_gather_offset_s16(const int16_t *, uint16x8_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vldrhq_gather_offset_s16)))
int16x8_t __arm_vldrhq_gather_offset(const int16_t *, uint16x8_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vldrhq_gather_offset_s32)))
int32x4_t __arm_vldrhq_gather_offset_s32(const int16_t *, uint32x4_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vldrhq_gather_offset_s32)))
int32x4_t __arm_vldrhq_gather_offset(const int16_t *, uint32x4_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vldrhq_gather_offset_u16)))
uint16x8_t __arm_vldrhq_gather_offset_u16(const uint16_t *, uint16x8_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vldrhq_gather_offset_u16)))
uint16x8_t __arm_vldrhq_gather_offset(const uint16_t *, uint16x8_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vldrhq_gather_offset_u32)))
uint32x4_t __arm_vldrhq_gather_offset_u32(const uint16_t *, uint32x4_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vldrhq_gather_offset_u32)))
uint32x4_t __arm_vldrhq_gather_offset(const uint16_t *, uint32x4_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vldrhq_gather_offset_z_s16)))
int16x8_t __arm_vldrhq_gather_offset_z_s16(const int16_t *, uint16x8_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vldrhq_gather_offset_z_s16)))
int16x8_t __arm_vldrhq_gather_offset_z(const int16_t *, uint16x8_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vldrhq_gather_offset_z_s32)))
int32x4_t __arm_vldrhq_gather_offset_z_s32(const int16_t *, uint32x4_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vldrhq_gather_offset_z_s32)))
int32x4_t __arm_vldrhq_gather_offset_z(const int16_t *, uint32x4_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vldrhq_gather_offset_z_u16)))
uint16x8_t __arm_vldrhq_gather_offset_z_u16(const uint16_t *, uint16x8_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vldrhq_gather_offset_z_u16)))
uint16x8_t __arm_vldrhq_gather_offset_z(const uint16_t *, uint16x8_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vldrhq_gather_offset_z_u32)))
uint32x4_t __arm_vldrhq_gather_offset_z_u32(const uint16_t *, uint32x4_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vldrhq_gather_offset_z_u32)))
uint32x4_t __arm_vldrhq_gather_offset_z(const uint16_t *, uint32x4_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vldrhq_gather_shifted_offset_s16)))
int16x8_t __arm_vldrhq_gather_shifted_offset_s16(const int16_t *, uint16x8_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vldrhq_gather_shifted_offset_s16)))
int16x8_t __arm_vldrhq_gather_shifted_offset(const int16_t *, uint16x8_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vldrhq_gather_shifted_offset_s32)))
int32x4_t __arm_vldrhq_gather_shifted_offset_s32(const int16_t *, uint32x4_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vldrhq_gather_shifted_offset_s32)))
int32x4_t __arm_vldrhq_gather_shifted_offset(const int16_t *, uint32x4_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vldrhq_gather_shifted_offset_u16)))
uint16x8_t __arm_vldrhq_gather_shifted_offset_u16(const uint16_t *, uint16x8_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vldrhq_gather_shifted_offset_u16)))
uint16x8_t __arm_vldrhq_gather_shifted_offset(const uint16_t *, uint16x8_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vldrhq_gather_shifted_offset_u32)))
uint32x4_t __arm_vldrhq_gather_shifted_offset_u32(const uint16_t *, uint32x4_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vldrhq_gather_shifted_offset_u32)))
uint32x4_t __arm_vldrhq_gather_shifted_offset(const uint16_t *, uint32x4_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vldrhq_gather_shifted_offset_z_s16)))
int16x8_t __arm_vldrhq_gather_shifted_offset_z_s16(const int16_t *, uint16x8_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vldrhq_gather_shifted_offset_z_s16)))
int16x8_t __arm_vldrhq_gather_shifted_offset_z(const int16_t *, uint16x8_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vldrhq_gather_shifted_offset_z_s32)))
int32x4_t __arm_vldrhq_gather_shifted_offset_z_s32(const int16_t *, uint32x4_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vldrhq_gather_shifted_offset_z_s32)))
int32x4_t __arm_vldrhq_gather_shifted_offset_z(const int16_t *, uint32x4_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vldrhq_gather_shifted_offset_z_u16)))
uint16x8_t __arm_vldrhq_gather_shifted_offset_z_u16(const uint16_t *, uint16x8_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vldrhq_gather_shifted_offset_z_u16)))
uint16x8_t __arm_vldrhq_gather_shifted_offset_z(const uint16_t *, uint16x8_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vldrhq_gather_shifted_offset_z_u32)))
uint32x4_t __arm_vldrhq_gather_shifted_offset_z_u32(const uint16_t *, uint32x4_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vldrhq_gather_shifted_offset_z_u32)))
uint32x4_t __arm_vldrhq_gather_shifted_offset_z(const uint16_t *, uint32x4_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vldrhq_s16)))
int16x8_t __arm_vldrhq_s16(const int16_t *);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vldrhq_s32)))
int32x4_t __arm_vldrhq_s32(const int16_t *);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vldrhq_u16)))
uint16x8_t __arm_vldrhq_u16(const uint16_t *);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vldrhq_u32)))
uint32x4_t __arm_vldrhq_u32(const uint16_t *);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vldrhq_z_s16)))
int16x8_t __arm_vldrhq_z_s16(const int16_t *, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vldrhq_z_s32)))
int32x4_t __arm_vldrhq_z_s32(const int16_t *, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vldrhq_z_u16)))
uint16x8_t __arm_vldrhq_z_u16(const uint16_t *, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vldrhq_z_u32)))
uint32x4_t __arm_vldrhq_z_u32(const uint16_t *, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vldrwq_gather_base_s32)))
int32x4_t __arm_vldrwq_gather_base_s32(uint32x4_t, int);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vldrwq_gather_base_u32)))
uint32x4_t __arm_vldrwq_gather_base_u32(uint32x4_t, int);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vldrwq_gather_base_wb_s32)))
int32x4_t __arm_vldrwq_gather_base_wb_s32(uint32x4_t *, int);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vldrwq_gather_base_wb_u32)))
uint32x4_t __arm_vldrwq_gather_base_wb_u32(uint32x4_t *, int);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vldrwq_gather_base_wb_z_s32)))
int32x4_t __arm_vldrwq_gather_base_wb_z_s32(uint32x4_t *, int, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vldrwq_gather_base_wb_z_u32)))
uint32x4_t __arm_vldrwq_gather_base_wb_z_u32(uint32x4_t *, int, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vldrwq_gather_base_z_s32)))
int32x4_t __arm_vldrwq_gather_base_z_s32(uint32x4_t, int, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vldrwq_gather_base_z_u32)))
uint32x4_t __arm_vldrwq_gather_base_z_u32(uint32x4_t, int, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vldrwq_gather_offset_s32)))
int32x4_t __arm_vldrwq_gather_offset_s32(const int32_t *, uint32x4_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vldrwq_gather_offset_s32)))
int32x4_t __arm_vldrwq_gather_offset(const int32_t *, uint32x4_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vldrwq_gather_offset_u32)))
uint32x4_t __arm_vldrwq_gather_offset_u32(const uint32_t *, uint32x4_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vldrwq_gather_offset_u32)))
uint32x4_t __arm_vldrwq_gather_offset(const uint32_t *, uint32x4_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vldrwq_gather_offset_z_s32)))
int32x4_t __arm_vldrwq_gather_offset_z_s32(const int32_t *, uint32x4_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vldrwq_gather_offset_z_s32)))
int32x4_t __arm_vldrwq_gather_offset_z(const int32_t *, uint32x4_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vldrwq_gather_offset_z_u32)))
uint32x4_t __arm_vldrwq_gather_offset_z_u32(const uint32_t *, uint32x4_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vldrwq_gather_offset_z_u32)))
uint32x4_t __arm_vldrwq_gather_offset_z(const uint32_t *, uint32x4_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vldrwq_gather_shifted_offset_s32)))
int32x4_t __arm_vldrwq_gather_shifted_offset_s32(const int32_t *, uint32x4_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vldrwq_gather_shifted_offset_s32)))
int32x4_t __arm_vldrwq_gather_shifted_offset(const int32_t *, uint32x4_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vldrwq_gather_shifted_offset_u32)))
uint32x4_t __arm_vldrwq_gather_shifted_offset_u32(const uint32_t *, uint32x4_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vldrwq_gather_shifted_offset_u32)))
uint32x4_t __arm_vldrwq_gather_shifted_offset(const uint32_t *, uint32x4_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vldrwq_gather_shifted_offset_z_s32)))
int32x4_t __arm_vldrwq_gather_shifted_offset_z_s32(const int32_t *, uint32x4_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vldrwq_gather_shifted_offset_z_s32)))
int32x4_t __arm_vldrwq_gather_shifted_offset_z(const int32_t *, uint32x4_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vldrwq_gather_shifted_offset_z_u32)))
uint32x4_t __arm_vldrwq_gather_shifted_offset_z_u32(const uint32_t *, uint32x4_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vldrwq_gather_shifted_offset_z_u32)))
uint32x4_t __arm_vldrwq_gather_shifted_offset_z(const uint32_t *, uint32x4_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vldrwq_s32)))
int32x4_t __arm_vldrwq_s32(const int32_t *);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vldrwq_u32)))
uint32x4_t __arm_vldrwq_u32(const uint32_t *);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vldrwq_z_s32)))
int32x4_t __arm_vldrwq_z_s32(const int32_t *, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vldrwq_z_u32)))
uint32x4_t __arm_vldrwq_z_u32(const uint32_t *, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vmaxvq_s16)))
int16_t __arm_vmaxvq_s16(int16_t, int16x8_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vmaxvq_s16)))
int16_t __arm_vmaxvq(int16_t, int16x8_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vmaxvq_s32)))
int32_t __arm_vmaxvq_s32(int32_t, int32x4_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vmaxvq_s32)))
int32_t __arm_vmaxvq(int32_t, int32x4_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vmaxvq_s8)))
int8_t __arm_vmaxvq_s8(int8_t, int8x16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vmaxvq_s8)))
int8_t __arm_vmaxvq(int8_t, int8x16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vmaxvq_u16)))
uint16_t __arm_vmaxvq_u16(uint16_t, uint16x8_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vmaxvq_u16)))
uint16_t __arm_vmaxvq(uint16_t, uint16x8_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vmaxvq_u32)))
uint32_t __arm_vmaxvq_u32(uint32_t, uint32x4_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vmaxvq_u32)))
uint32_t __arm_vmaxvq(uint32_t, uint32x4_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vmaxvq_u8)))
uint8_t __arm_vmaxvq_u8(uint8_t, uint8x16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vmaxvq_u8)))
uint8_t __arm_vmaxvq(uint8_t, uint8x16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vminvq_s16)))
int16_t __arm_vminvq_s16(int16_t, int16x8_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vminvq_s16)))
int16_t __arm_vminvq(int16_t, int16x8_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vminvq_s32)))
int32_t __arm_vminvq_s32(int32_t, int32x4_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vminvq_s32)))
int32_t __arm_vminvq(int32_t, int32x4_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vminvq_s8)))
int8_t __arm_vminvq_s8(int8_t, int8x16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vminvq_s8)))
int8_t __arm_vminvq(int8_t, int8x16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vminvq_u16)))
uint16_t __arm_vminvq_u16(uint16_t, uint16x8_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vminvq_u16)))
uint16_t __arm_vminvq(uint16_t, uint16x8_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vminvq_u32)))
uint32_t __arm_vminvq_u32(uint32_t, uint32x4_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vminvq_u32)))
uint32_t __arm_vminvq(uint32_t, uint32x4_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vminvq_u8)))
uint8_t __arm_vminvq_u8(uint8_t, uint8x16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vminvq_u8)))
uint8_t __arm_vminvq(uint8_t, uint8x16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vmulq_m_s16)))
int16x8_t __arm_vmulq_m_s16(int16x8_t, int16x8_t, int16x8_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vmulq_m_s16)))
int16x8_t __arm_vmulq_m(int16x8_t, int16x8_t, int16x8_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vmulq_m_s32)))
int32x4_t __arm_vmulq_m_s32(int32x4_t, int32x4_t, int32x4_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vmulq_m_s32)))
int32x4_t __arm_vmulq_m(int32x4_t, int32x4_t, int32x4_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vmulq_m_s8)))
int8x16_t __arm_vmulq_m_s8(int8x16_t, int8x16_t, int8x16_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vmulq_m_s8)))
int8x16_t __arm_vmulq_m(int8x16_t, int8x16_t, int8x16_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vmulq_m_u16)))
uint16x8_t __arm_vmulq_m_u16(uint16x8_t, uint16x8_t, uint16x8_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vmulq_m_u16)))
uint16x8_t __arm_vmulq_m(uint16x8_t, uint16x8_t, uint16x8_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vmulq_m_u32)))
uint32x4_t __arm_vmulq_m_u32(uint32x4_t, uint32x4_t, uint32x4_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vmulq_m_u32)))
uint32x4_t __arm_vmulq_m(uint32x4_t, uint32x4_t, uint32x4_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vmulq_m_u8)))
uint8x16_t __arm_vmulq_m_u8(uint8x16_t, uint8x16_t, uint8x16_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vmulq_m_u8)))
uint8x16_t __arm_vmulq_m(uint8x16_t, uint8x16_t, uint8x16_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vmulq_s16)))
int16x8_t __arm_vmulq_s16(int16x8_t, int16x8_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vmulq_s16)))
int16x8_t __arm_vmulq(int16x8_t, int16x8_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vmulq_s32)))
int32x4_t __arm_vmulq_s32(int32x4_t, int32x4_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vmulq_s32)))
int32x4_t __arm_vmulq(int32x4_t, int32x4_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vmulq_s8)))
int8x16_t __arm_vmulq_s8(int8x16_t, int8x16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vmulq_s8)))
int8x16_t __arm_vmulq(int8x16_t, int8x16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vmulq_u16)))
uint16x8_t __arm_vmulq_u16(uint16x8_t, uint16x8_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vmulq_u16)))
uint16x8_t __arm_vmulq(uint16x8_t, uint16x8_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vmulq_u32)))
uint32x4_t __arm_vmulq_u32(uint32x4_t, uint32x4_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vmulq_u32)))
uint32x4_t __arm_vmulq(uint32x4_t, uint32x4_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vmulq_u8)))
uint8x16_t __arm_vmulq_u8(uint8x16_t, uint8x16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vmulq_u8)))
uint8x16_t __arm_vmulq(uint8x16_t, uint8x16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vornq_m_s16)))
int16x8_t __arm_vornq_m_s16(int16x8_t, int16x8_t, int16x8_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vornq_m_s16)))
int16x8_t __arm_vornq_m(int16x8_t, int16x8_t, int16x8_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vornq_m_s32)))
int32x4_t __arm_vornq_m_s32(int32x4_t, int32x4_t, int32x4_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vornq_m_s32)))
int32x4_t __arm_vornq_m(int32x4_t, int32x4_t, int32x4_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vornq_m_s8)))
int8x16_t __arm_vornq_m_s8(int8x16_t, int8x16_t, int8x16_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vornq_m_s8)))
int8x16_t __arm_vornq_m(int8x16_t, int8x16_t, int8x16_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vornq_m_u16)))
uint16x8_t __arm_vornq_m_u16(uint16x8_t, uint16x8_t, uint16x8_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vornq_m_u16)))
uint16x8_t __arm_vornq_m(uint16x8_t, uint16x8_t, uint16x8_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vornq_m_u32)))
uint32x4_t __arm_vornq_m_u32(uint32x4_t, uint32x4_t, uint32x4_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vornq_m_u32)))
uint32x4_t __arm_vornq_m(uint32x4_t, uint32x4_t, uint32x4_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vornq_m_u8)))
uint8x16_t __arm_vornq_m_u8(uint8x16_t, uint8x16_t, uint8x16_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vornq_m_u8)))
uint8x16_t __arm_vornq_m(uint8x16_t, uint8x16_t, uint8x16_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vornq_s16)))
int16x8_t __arm_vornq_s16(int16x8_t, int16x8_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vornq_s16)))
int16x8_t __arm_vornq(int16x8_t, int16x8_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vornq_s32)))
int32x4_t __arm_vornq_s32(int32x4_t, int32x4_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vornq_s32)))
int32x4_t __arm_vornq(int32x4_t, int32x4_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vornq_s8)))
int8x16_t __arm_vornq_s8(int8x16_t, int8x16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vornq_s8)))
int8x16_t __arm_vornq(int8x16_t, int8x16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vornq_u16)))
uint16x8_t __arm_vornq_u16(uint16x8_t, uint16x8_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vornq_u16)))
uint16x8_t __arm_vornq(uint16x8_t, uint16x8_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vornq_u32)))
uint32x4_t __arm_vornq_u32(uint32x4_t, uint32x4_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vornq_u32)))
uint32x4_t __arm_vornq(uint32x4_t, uint32x4_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vornq_u8)))
uint8x16_t __arm_vornq_u8(uint8x16_t, uint8x16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vornq_u8)))
uint8x16_t __arm_vornq(uint8x16_t, uint8x16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vorrq_m_s16)))
int16x8_t __arm_vorrq_m_s16(int16x8_t, int16x8_t, int16x8_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vorrq_m_s16)))
int16x8_t __arm_vorrq_m(int16x8_t, int16x8_t, int16x8_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vorrq_m_s32)))
int32x4_t __arm_vorrq_m_s32(int32x4_t, int32x4_t, int32x4_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vorrq_m_s32)))
int32x4_t __arm_vorrq_m(int32x4_t, int32x4_t, int32x4_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vorrq_m_s8)))
int8x16_t __arm_vorrq_m_s8(int8x16_t, int8x16_t, int8x16_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vorrq_m_s8)))
int8x16_t __arm_vorrq_m(int8x16_t, int8x16_t, int8x16_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vorrq_m_u16)))
uint16x8_t __arm_vorrq_m_u16(uint16x8_t, uint16x8_t, uint16x8_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vorrq_m_u16)))
uint16x8_t __arm_vorrq_m(uint16x8_t, uint16x8_t, uint16x8_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vorrq_m_u32)))
uint32x4_t __arm_vorrq_m_u32(uint32x4_t, uint32x4_t, uint32x4_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vorrq_m_u32)))
uint32x4_t __arm_vorrq_m(uint32x4_t, uint32x4_t, uint32x4_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vorrq_m_u8)))
uint8x16_t __arm_vorrq_m_u8(uint8x16_t, uint8x16_t, uint8x16_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vorrq_m_u8)))
uint8x16_t __arm_vorrq_m(uint8x16_t, uint8x16_t, uint8x16_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vorrq_s16)))
int16x8_t __arm_vorrq_s16(int16x8_t, int16x8_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vorrq_s16)))
int16x8_t __arm_vorrq(int16x8_t, int16x8_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vorrq_s32)))
int32x4_t __arm_vorrq_s32(int32x4_t, int32x4_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vorrq_s32)))
int32x4_t __arm_vorrq(int32x4_t, int32x4_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vorrq_s8)))
int8x16_t __arm_vorrq_s8(int8x16_t, int8x16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vorrq_s8)))
int8x16_t __arm_vorrq(int8x16_t, int8x16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vorrq_u16)))
uint16x8_t __arm_vorrq_u16(uint16x8_t, uint16x8_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vorrq_u16)))
uint16x8_t __arm_vorrq(uint16x8_t, uint16x8_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vorrq_u32)))
uint32x4_t __arm_vorrq_u32(uint32x4_t, uint32x4_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vorrq_u32)))
uint32x4_t __arm_vorrq(uint32x4_t, uint32x4_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vorrq_u8)))
uint8x16_t __arm_vorrq_u8(uint8x16_t, uint8x16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vorrq_u8)))
uint8x16_t __arm_vorrq(uint8x16_t, uint8x16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_s16_s32)))
int16x8_t __arm_vreinterpretq_s16_s32(int32x4_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_s16_s32)))
int16x8_t __arm_vreinterpretq_s16(int32x4_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_s16_s64)))
int16x8_t __arm_vreinterpretq_s16_s64(int64x2_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_s16_s64)))
int16x8_t __arm_vreinterpretq_s16(int64x2_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_s16_s8)))
int16x8_t __arm_vreinterpretq_s16_s8(int8x16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_s16_s8)))
int16x8_t __arm_vreinterpretq_s16(int8x16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_s16_u16)))
int16x8_t __arm_vreinterpretq_s16_u16(uint16x8_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_s16_u16)))
int16x8_t __arm_vreinterpretq_s16(uint16x8_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_s16_u32)))
int16x8_t __arm_vreinterpretq_s16_u32(uint32x4_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_s16_u32)))
int16x8_t __arm_vreinterpretq_s16(uint32x4_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_s16_u64)))
int16x8_t __arm_vreinterpretq_s16_u64(uint64x2_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_s16_u64)))
int16x8_t __arm_vreinterpretq_s16(uint64x2_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_s16_u8)))
int16x8_t __arm_vreinterpretq_s16_u8(uint8x16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_s16_u8)))
int16x8_t __arm_vreinterpretq_s16(uint8x16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_s32_s16)))
int32x4_t __arm_vreinterpretq_s32_s16(int16x8_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_s32_s16)))
int32x4_t __arm_vreinterpretq_s32(int16x8_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_s32_s64)))
int32x4_t __arm_vreinterpretq_s32_s64(int64x2_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_s32_s64)))
int32x4_t __arm_vreinterpretq_s32(int64x2_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_s32_s8)))
int32x4_t __arm_vreinterpretq_s32_s8(int8x16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_s32_s8)))
int32x4_t __arm_vreinterpretq_s32(int8x16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_s32_u16)))
int32x4_t __arm_vreinterpretq_s32_u16(uint16x8_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_s32_u16)))
int32x4_t __arm_vreinterpretq_s32(uint16x8_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_s32_u32)))
int32x4_t __arm_vreinterpretq_s32_u32(uint32x4_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_s32_u32)))
int32x4_t __arm_vreinterpretq_s32(uint32x4_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_s32_u64)))
int32x4_t __arm_vreinterpretq_s32_u64(uint64x2_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_s32_u64)))
int32x4_t __arm_vreinterpretq_s32(uint64x2_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_s32_u8)))
int32x4_t __arm_vreinterpretq_s32_u8(uint8x16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_s32_u8)))
int32x4_t __arm_vreinterpretq_s32(uint8x16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_s64_s16)))
int64x2_t __arm_vreinterpretq_s64_s16(int16x8_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_s64_s16)))
int64x2_t __arm_vreinterpretq_s64(int16x8_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_s64_s32)))
int64x2_t __arm_vreinterpretq_s64_s32(int32x4_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_s64_s32)))
int64x2_t __arm_vreinterpretq_s64(int32x4_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_s64_s8)))
int64x2_t __arm_vreinterpretq_s64_s8(int8x16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_s64_s8)))
int64x2_t __arm_vreinterpretq_s64(int8x16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_s64_u16)))
int64x2_t __arm_vreinterpretq_s64_u16(uint16x8_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_s64_u16)))
int64x2_t __arm_vreinterpretq_s64(uint16x8_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_s64_u32)))
int64x2_t __arm_vreinterpretq_s64_u32(uint32x4_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_s64_u32)))
int64x2_t __arm_vreinterpretq_s64(uint32x4_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_s64_u64)))
int64x2_t __arm_vreinterpretq_s64_u64(uint64x2_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_s64_u64)))
int64x2_t __arm_vreinterpretq_s64(uint64x2_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_s64_u8)))
int64x2_t __arm_vreinterpretq_s64_u8(uint8x16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_s64_u8)))
int64x2_t __arm_vreinterpretq_s64(uint8x16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_s8_s16)))
int8x16_t __arm_vreinterpretq_s8_s16(int16x8_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_s8_s16)))
int8x16_t __arm_vreinterpretq_s8(int16x8_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_s8_s32)))
int8x16_t __arm_vreinterpretq_s8_s32(int32x4_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_s8_s32)))
int8x16_t __arm_vreinterpretq_s8(int32x4_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_s8_s64)))
int8x16_t __arm_vreinterpretq_s8_s64(int64x2_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_s8_s64)))
int8x16_t __arm_vreinterpretq_s8(int64x2_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_s8_u16)))
int8x16_t __arm_vreinterpretq_s8_u16(uint16x8_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_s8_u16)))
int8x16_t __arm_vreinterpretq_s8(uint16x8_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_s8_u32)))
int8x16_t __arm_vreinterpretq_s8_u32(uint32x4_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_s8_u32)))
int8x16_t __arm_vreinterpretq_s8(uint32x4_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_s8_u64)))
int8x16_t __arm_vreinterpretq_s8_u64(uint64x2_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_s8_u64)))
int8x16_t __arm_vreinterpretq_s8(uint64x2_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_s8_u8)))
int8x16_t __arm_vreinterpretq_s8_u8(uint8x16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_s8_u8)))
int8x16_t __arm_vreinterpretq_s8(uint8x16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_u16_s16)))
uint16x8_t __arm_vreinterpretq_u16_s16(int16x8_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_u16_s16)))
uint16x8_t __arm_vreinterpretq_u16(int16x8_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_u16_s32)))
uint16x8_t __arm_vreinterpretq_u16_s32(int32x4_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_u16_s32)))
uint16x8_t __arm_vreinterpretq_u16(int32x4_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_u16_s64)))
uint16x8_t __arm_vreinterpretq_u16_s64(int64x2_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_u16_s64)))
uint16x8_t __arm_vreinterpretq_u16(int64x2_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_u16_s8)))
uint16x8_t __arm_vreinterpretq_u16_s8(int8x16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_u16_s8)))
uint16x8_t __arm_vreinterpretq_u16(int8x16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_u16_u32)))
uint16x8_t __arm_vreinterpretq_u16_u32(uint32x4_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_u16_u32)))
uint16x8_t __arm_vreinterpretq_u16(uint32x4_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_u16_u64)))
uint16x8_t __arm_vreinterpretq_u16_u64(uint64x2_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_u16_u64)))
uint16x8_t __arm_vreinterpretq_u16(uint64x2_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_u16_u8)))
uint16x8_t __arm_vreinterpretq_u16_u8(uint8x16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_u16_u8)))
uint16x8_t __arm_vreinterpretq_u16(uint8x16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_u32_s16)))
uint32x4_t __arm_vreinterpretq_u32_s16(int16x8_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_u32_s16)))
uint32x4_t __arm_vreinterpretq_u32(int16x8_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_u32_s32)))
uint32x4_t __arm_vreinterpretq_u32_s32(int32x4_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_u32_s32)))
uint32x4_t __arm_vreinterpretq_u32(int32x4_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_u32_s64)))
uint32x4_t __arm_vreinterpretq_u32_s64(int64x2_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_u32_s64)))
uint32x4_t __arm_vreinterpretq_u32(int64x2_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_u32_s8)))
uint32x4_t __arm_vreinterpretq_u32_s8(int8x16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_u32_s8)))
uint32x4_t __arm_vreinterpretq_u32(int8x16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_u32_u16)))
uint32x4_t __arm_vreinterpretq_u32_u16(uint16x8_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_u32_u16)))
uint32x4_t __arm_vreinterpretq_u32(uint16x8_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_u32_u64)))
uint32x4_t __arm_vreinterpretq_u32_u64(uint64x2_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_u32_u64)))
uint32x4_t __arm_vreinterpretq_u32(uint64x2_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_u32_u8)))
uint32x4_t __arm_vreinterpretq_u32_u8(uint8x16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_u32_u8)))
uint32x4_t __arm_vreinterpretq_u32(uint8x16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_u64_s16)))
uint64x2_t __arm_vreinterpretq_u64_s16(int16x8_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_u64_s16)))
uint64x2_t __arm_vreinterpretq_u64(int16x8_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_u64_s32)))
uint64x2_t __arm_vreinterpretq_u64_s32(int32x4_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_u64_s32)))
uint64x2_t __arm_vreinterpretq_u64(int32x4_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_u64_s64)))
uint64x2_t __arm_vreinterpretq_u64_s64(int64x2_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_u64_s64)))
uint64x2_t __arm_vreinterpretq_u64(int64x2_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_u64_s8)))
uint64x2_t __arm_vreinterpretq_u64_s8(int8x16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_u64_s8)))
uint64x2_t __arm_vreinterpretq_u64(int8x16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_u64_u16)))
uint64x2_t __arm_vreinterpretq_u64_u16(uint16x8_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_u64_u16)))
uint64x2_t __arm_vreinterpretq_u64(uint16x8_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_u64_u32)))
uint64x2_t __arm_vreinterpretq_u64_u32(uint32x4_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_u64_u32)))
uint64x2_t __arm_vreinterpretq_u64(uint32x4_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_u64_u8)))
uint64x2_t __arm_vreinterpretq_u64_u8(uint8x16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_u64_u8)))
uint64x2_t __arm_vreinterpretq_u64(uint8x16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_u8_s16)))
uint8x16_t __arm_vreinterpretq_u8_s16(int16x8_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_u8_s16)))
uint8x16_t __arm_vreinterpretq_u8(int16x8_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_u8_s32)))
uint8x16_t __arm_vreinterpretq_u8_s32(int32x4_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_u8_s32)))
uint8x16_t __arm_vreinterpretq_u8(int32x4_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_u8_s64)))
uint8x16_t __arm_vreinterpretq_u8_s64(int64x2_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_u8_s64)))
uint8x16_t __arm_vreinterpretq_u8(int64x2_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_u8_s8)))
uint8x16_t __arm_vreinterpretq_u8_s8(int8x16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_u8_s8)))
uint8x16_t __arm_vreinterpretq_u8(int8x16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_u8_u16)))
uint8x16_t __arm_vreinterpretq_u8_u16(uint16x8_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_u8_u16)))
uint8x16_t __arm_vreinterpretq_u8(uint16x8_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_u8_u32)))
uint8x16_t __arm_vreinterpretq_u8_u32(uint32x4_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_u8_u32)))
uint8x16_t __arm_vreinterpretq_u8(uint32x4_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_u8_u64)))
uint8x16_t __arm_vreinterpretq_u8_u64(uint64x2_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_u8_u64)))
uint8x16_t __arm_vreinterpretq_u8(uint64x2_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vsetq_lane_s16)))
int16x8_t __arm_vsetq_lane_s16(int16_t, int16x8_t, int);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vsetq_lane_s16)))
int16x8_t __arm_vsetq_lane(int16_t, int16x8_t, int);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vsetq_lane_s32)))
int32x4_t __arm_vsetq_lane_s32(int32_t, int32x4_t, int);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vsetq_lane_s32)))
int32x4_t __arm_vsetq_lane(int32_t, int32x4_t, int);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vsetq_lane_s64)))
int64x2_t __arm_vsetq_lane_s64(int64_t, int64x2_t, int);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vsetq_lane_s64)))
int64x2_t __arm_vsetq_lane(int64_t, int64x2_t, int);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vsetq_lane_s8)))
int8x16_t __arm_vsetq_lane_s8(int8_t, int8x16_t, int);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vsetq_lane_s8)))
int8x16_t __arm_vsetq_lane(int8_t, int8x16_t, int);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vsetq_lane_u16)))
uint16x8_t __arm_vsetq_lane_u16(uint16_t, uint16x8_t, int);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vsetq_lane_u16)))
uint16x8_t __arm_vsetq_lane(uint16_t, uint16x8_t, int);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vsetq_lane_u32)))
uint32x4_t __arm_vsetq_lane_u32(uint32_t, uint32x4_t, int);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vsetq_lane_u32)))
uint32x4_t __arm_vsetq_lane(uint32_t, uint32x4_t, int);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vsetq_lane_u64)))
uint64x2_t __arm_vsetq_lane_u64(uint64_t, uint64x2_t, int);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vsetq_lane_u64)))
uint64x2_t __arm_vsetq_lane(uint64_t, uint64x2_t, int);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vsetq_lane_u8)))
uint8x16_t __arm_vsetq_lane_u8(uint8_t, uint8x16_t, int);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vsetq_lane_u8)))
uint8x16_t __arm_vsetq_lane(uint8_t, uint8x16_t, int);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vst1q_p_s16)))
void __arm_vst1q_p_s16(int16_t *, int16x8_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vst1q_p_s16)))
void __arm_vst1q_p(int16_t *, int16x8_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vst1q_p_s32)))
void __arm_vst1q_p_s32(int32_t *, int32x4_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vst1q_p_s32)))
void __arm_vst1q_p(int32_t *, int32x4_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vst1q_p_s8)))
void __arm_vst1q_p_s8(int8_t *, int8x16_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vst1q_p_s8)))
void __arm_vst1q_p(int8_t *, int8x16_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vst1q_p_u16)))
void __arm_vst1q_p_u16(uint16_t *, uint16x8_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vst1q_p_u16)))
void __arm_vst1q_p(uint16_t *, uint16x8_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vst1q_p_u32)))
void __arm_vst1q_p_u32(uint32_t *, uint32x4_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vst1q_p_u32)))
void __arm_vst1q_p(uint32_t *, uint32x4_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vst1q_p_u8)))
void __arm_vst1q_p_u8(uint8_t *, uint8x16_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vst1q_p_u8)))
void __arm_vst1q_p(uint8_t *, uint8x16_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vst1q_s16)))
void __arm_vst1q_s16(int16_t *, int16x8_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vst1q_s16)))
void __arm_vst1q(int16_t *, int16x8_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vst1q_s32)))
void __arm_vst1q_s32(int32_t *, int32x4_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vst1q_s32)))
void __arm_vst1q(int32_t *, int32x4_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vst1q_s8)))
void __arm_vst1q_s8(int8_t *, int8x16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vst1q_s8)))
void __arm_vst1q(int8_t *, int8x16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vst1q_u16)))
void __arm_vst1q_u16(uint16_t *, uint16x8_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vst1q_u16)))
void __arm_vst1q(uint16_t *, uint16x8_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vst1q_u32)))
void __arm_vst1q_u32(uint32_t *, uint32x4_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vst1q_u32)))
void __arm_vst1q(uint32_t *, uint32x4_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vst1q_u8)))
void __arm_vst1q_u8(uint8_t *, uint8x16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vst1q_u8)))
void __arm_vst1q(uint8_t *, uint8x16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vst2q_s16)))
void __arm_vst2q_s16(int16_t *, int16x8x2_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vst2q_s16)))
void __arm_vst2q(int16_t *, int16x8x2_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vst2q_s32)))
void __arm_vst2q_s32(int32_t *, int32x4x2_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vst2q_s32)))
void __arm_vst2q(int32_t *, int32x4x2_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vst2q_s8)))
void __arm_vst2q_s8(int8_t *, int8x16x2_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vst2q_s8)))
void __arm_vst2q(int8_t *, int8x16x2_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vst2q_u16)))
void __arm_vst2q_u16(uint16_t *, uint16x8x2_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vst2q_u16)))
void __arm_vst2q(uint16_t *, uint16x8x2_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vst2q_u32)))
void __arm_vst2q_u32(uint32_t *, uint32x4x2_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vst2q_u32)))
void __arm_vst2q(uint32_t *, uint32x4x2_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vst2q_u8)))
void __arm_vst2q_u8(uint8_t *, uint8x16x2_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vst2q_u8)))
void __arm_vst2q(uint8_t *, uint8x16x2_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vst4q_s16)))
void __arm_vst4q_s16(int16_t *, int16x8x4_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vst4q_s16)))
void __arm_vst4q(int16_t *, int16x8x4_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vst4q_s32)))
void __arm_vst4q_s32(int32_t *, int32x4x4_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vst4q_s32)))
void __arm_vst4q(int32_t *, int32x4x4_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vst4q_s8)))
void __arm_vst4q_s8(int8_t *, int8x16x4_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vst4q_s8)))
void __arm_vst4q(int8_t *, int8x16x4_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vst4q_u16)))
void __arm_vst4q_u16(uint16_t *, uint16x8x4_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vst4q_u16)))
void __arm_vst4q(uint16_t *, uint16x8x4_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vst4q_u32)))
void __arm_vst4q_u32(uint32_t *, uint32x4x4_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vst4q_u32)))
void __arm_vst4q(uint32_t *, uint32x4x4_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vst4q_u8)))
void __arm_vst4q_u8(uint8_t *, uint8x16x4_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vst4q_u8)))
void __arm_vst4q(uint8_t *, uint8x16x4_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vstrbq_p_s16)))
void __arm_vstrbq_p_s16(int8_t *, int16x8_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vstrbq_p_s16)))
void __arm_vstrbq_p(int8_t *, int16x8_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vstrbq_p_s32)))
void __arm_vstrbq_p_s32(int8_t *, int32x4_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vstrbq_p_s32)))
void __arm_vstrbq_p(int8_t *, int32x4_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vstrbq_p_s8)))
void __arm_vstrbq_p_s8(int8_t *, int8x16_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vstrbq_p_s8)))
void __arm_vstrbq_p(int8_t *, int8x16_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vstrbq_p_u16)))
void __arm_vstrbq_p_u16(uint8_t *, uint16x8_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vstrbq_p_u16)))
void __arm_vstrbq_p(uint8_t *, uint16x8_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vstrbq_p_u32)))
void __arm_vstrbq_p_u32(uint8_t *, uint32x4_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vstrbq_p_u32)))
void __arm_vstrbq_p(uint8_t *, uint32x4_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vstrbq_p_u8)))
void __arm_vstrbq_p_u8(uint8_t *, uint8x16_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vstrbq_p_u8)))
void __arm_vstrbq_p(uint8_t *, uint8x16_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vstrbq_s16)))
void __arm_vstrbq_s16(int8_t *, int16x8_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vstrbq_s16)))
void __arm_vstrbq(int8_t *, int16x8_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vstrbq_s32)))
void __arm_vstrbq_s32(int8_t *, int32x4_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vstrbq_s32)))
void __arm_vstrbq(int8_t *, int32x4_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vstrbq_s8)))
void __arm_vstrbq_s8(int8_t *, int8x16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vstrbq_s8)))
void __arm_vstrbq(int8_t *, int8x16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vstrbq_scatter_offset_p_s16)))
void __arm_vstrbq_scatter_offset_p_s16(int8_t *, uint16x8_t, int16x8_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vstrbq_scatter_offset_p_s16)))
void __arm_vstrbq_scatter_offset_p(int8_t *, uint16x8_t, int16x8_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vstrbq_scatter_offset_p_s32)))
void __arm_vstrbq_scatter_offset_p_s32(int8_t *, uint32x4_t, int32x4_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vstrbq_scatter_offset_p_s32)))
void __arm_vstrbq_scatter_offset_p(int8_t *, uint32x4_t, int32x4_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vstrbq_scatter_offset_p_s8)))
void __arm_vstrbq_scatter_offset_p_s8(int8_t *, uint8x16_t, int8x16_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vstrbq_scatter_offset_p_s8)))
void __arm_vstrbq_scatter_offset_p(int8_t *, uint8x16_t, int8x16_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vstrbq_scatter_offset_p_u16)))
void __arm_vstrbq_scatter_offset_p_u16(uint8_t *, uint16x8_t, uint16x8_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vstrbq_scatter_offset_p_u16)))
void __arm_vstrbq_scatter_offset_p(uint8_t *, uint16x8_t, uint16x8_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vstrbq_scatter_offset_p_u32)))
void __arm_vstrbq_scatter_offset_p_u32(uint8_t *, uint32x4_t, uint32x4_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vstrbq_scatter_offset_p_u32)))
void __arm_vstrbq_scatter_offset_p(uint8_t *, uint32x4_t, uint32x4_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vstrbq_scatter_offset_p_u8)))
void __arm_vstrbq_scatter_offset_p_u8(uint8_t *, uint8x16_t, uint8x16_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vstrbq_scatter_offset_p_u8)))
void __arm_vstrbq_scatter_offset_p(uint8_t *, uint8x16_t, uint8x16_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vstrbq_scatter_offset_s16)))
void __arm_vstrbq_scatter_offset_s16(int8_t *, uint16x8_t, int16x8_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vstrbq_scatter_offset_s16)))
void __arm_vstrbq_scatter_offset(int8_t *, uint16x8_t, int16x8_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vstrbq_scatter_offset_s32)))
void __arm_vstrbq_scatter_offset_s32(int8_t *, uint32x4_t, int32x4_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vstrbq_scatter_offset_s32)))
void __arm_vstrbq_scatter_offset(int8_t *, uint32x4_t, int32x4_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vstrbq_scatter_offset_s8)))
void __arm_vstrbq_scatter_offset_s8(int8_t *, uint8x16_t, int8x16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vstrbq_scatter_offset_s8)))
void __arm_vstrbq_scatter_offset(int8_t *, uint8x16_t, int8x16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vstrbq_scatter_offset_u16)))
void __arm_vstrbq_scatter_offset_u16(uint8_t *, uint16x8_t, uint16x8_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vstrbq_scatter_offset_u16)))
void __arm_vstrbq_scatter_offset(uint8_t *, uint16x8_t, uint16x8_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vstrbq_scatter_offset_u32)))
void __arm_vstrbq_scatter_offset_u32(uint8_t *, uint32x4_t, uint32x4_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vstrbq_scatter_offset_u32)))
void __arm_vstrbq_scatter_offset(uint8_t *, uint32x4_t, uint32x4_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vstrbq_scatter_offset_u8)))
void __arm_vstrbq_scatter_offset_u8(uint8_t *, uint8x16_t, uint8x16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vstrbq_scatter_offset_u8)))
void __arm_vstrbq_scatter_offset(uint8_t *, uint8x16_t, uint8x16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vstrbq_u16)))
void __arm_vstrbq_u16(uint8_t *, uint16x8_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vstrbq_u16)))
void __arm_vstrbq(uint8_t *, uint16x8_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vstrbq_u32)))
void __arm_vstrbq_u32(uint8_t *, uint32x4_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vstrbq_u32)))
void __arm_vstrbq(uint8_t *, uint32x4_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vstrbq_u8)))
void __arm_vstrbq_u8(uint8_t *, uint8x16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vstrbq_u8)))
void __arm_vstrbq(uint8_t *, uint8x16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vstrdq_scatter_base_p_s64)))
void __arm_vstrdq_scatter_base_p_s64(uint64x2_t, int, int64x2_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vstrdq_scatter_base_p_s64)))
void __arm_vstrdq_scatter_base_p(uint64x2_t, int, int64x2_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vstrdq_scatter_base_p_u64)))
void __arm_vstrdq_scatter_base_p_u64(uint64x2_t, int, uint64x2_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vstrdq_scatter_base_p_u64)))
void __arm_vstrdq_scatter_base_p(uint64x2_t, int, uint64x2_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vstrdq_scatter_base_s64)))
void __arm_vstrdq_scatter_base_s64(uint64x2_t, int, int64x2_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vstrdq_scatter_base_s64)))
void __arm_vstrdq_scatter_base(uint64x2_t, int, int64x2_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vstrdq_scatter_base_u64)))
void __arm_vstrdq_scatter_base_u64(uint64x2_t, int, uint64x2_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vstrdq_scatter_base_u64)))
void __arm_vstrdq_scatter_base(uint64x2_t, int, uint64x2_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vstrdq_scatter_base_wb_p_s64)))
void __arm_vstrdq_scatter_base_wb_p_s64(uint64x2_t *, int, int64x2_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vstrdq_scatter_base_wb_p_s64)))
void __arm_vstrdq_scatter_base_wb_p(uint64x2_t *, int, int64x2_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vstrdq_scatter_base_wb_p_u64)))
void __arm_vstrdq_scatter_base_wb_p_u64(uint64x2_t *, int, uint64x2_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vstrdq_scatter_base_wb_p_u64)))
void __arm_vstrdq_scatter_base_wb_p(uint64x2_t *, int, uint64x2_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vstrdq_scatter_base_wb_s64)))
void __arm_vstrdq_scatter_base_wb_s64(uint64x2_t *, int, int64x2_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vstrdq_scatter_base_wb_s64)))
void __arm_vstrdq_scatter_base_wb(uint64x2_t *, int, int64x2_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vstrdq_scatter_base_wb_u64)))
void __arm_vstrdq_scatter_base_wb_u64(uint64x2_t *, int, uint64x2_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vstrdq_scatter_base_wb_u64)))
void __arm_vstrdq_scatter_base_wb(uint64x2_t *, int, uint64x2_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vstrdq_scatter_offset_p_s64)))
void __arm_vstrdq_scatter_offset_p_s64(int64_t *, uint64x2_t, int64x2_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vstrdq_scatter_offset_p_s64)))
void __arm_vstrdq_scatter_offset_p(int64_t *, uint64x2_t, int64x2_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vstrdq_scatter_offset_p_u64)))
void __arm_vstrdq_scatter_offset_p_u64(uint64_t *, uint64x2_t, uint64x2_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vstrdq_scatter_offset_p_u64)))
void __arm_vstrdq_scatter_offset_p(uint64_t *, uint64x2_t, uint64x2_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vstrdq_scatter_offset_s64)))
void __arm_vstrdq_scatter_offset_s64(int64_t *, uint64x2_t, int64x2_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vstrdq_scatter_offset_s64)))
void __arm_vstrdq_scatter_offset(int64_t *, uint64x2_t, int64x2_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vstrdq_scatter_offset_u64)))
void __arm_vstrdq_scatter_offset_u64(uint64_t *, uint64x2_t, uint64x2_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vstrdq_scatter_offset_u64)))
void __arm_vstrdq_scatter_offset(uint64_t *, uint64x2_t, uint64x2_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vstrdq_scatter_shifted_offset_p_s64)))
void __arm_vstrdq_scatter_shifted_offset_p_s64(int64_t *, uint64x2_t, int64x2_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vstrdq_scatter_shifted_offset_p_s64)))
void __arm_vstrdq_scatter_shifted_offset_p(int64_t *, uint64x2_t, int64x2_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vstrdq_scatter_shifted_offset_p_u64)))
void __arm_vstrdq_scatter_shifted_offset_p_u64(uint64_t *, uint64x2_t, uint64x2_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vstrdq_scatter_shifted_offset_p_u64)))
void __arm_vstrdq_scatter_shifted_offset_p(uint64_t *, uint64x2_t, uint64x2_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vstrdq_scatter_shifted_offset_s64)))
void __arm_vstrdq_scatter_shifted_offset_s64(int64_t *, uint64x2_t, int64x2_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vstrdq_scatter_shifted_offset_s64)))
void __arm_vstrdq_scatter_shifted_offset(int64_t *, uint64x2_t, int64x2_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vstrdq_scatter_shifted_offset_u64)))
void __arm_vstrdq_scatter_shifted_offset_u64(uint64_t *, uint64x2_t, uint64x2_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vstrdq_scatter_shifted_offset_u64)))
void __arm_vstrdq_scatter_shifted_offset(uint64_t *, uint64x2_t, uint64x2_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vstrhq_p_s16)))
void __arm_vstrhq_p_s16(int16_t *, int16x8_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vstrhq_p_s16)))
void __arm_vstrhq_p(int16_t *, int16x8_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vstrhq_p_s32)))
void __arm_vstrhq_p_s32(int16_t *, int32x4_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vstrhq_p_s32)))
void __arm_vstrhq_p(int16_t *, int32x4_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vstrhq_p_u16)))
void __arm_vstrhq_p_u16(uint16_t *, uint16x8_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vstrhq_p_u16)))
void __arm_vstrhq_p(uint16_t *, uint16x8_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vstrhq_p_u32)))
void __arm_vstrhq_p_u32(uint16_t *, uint32x4_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vstrhq_p_u32)))
void __arm_vstrhq_p(uint16_t *, uint32x4_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vstrhq_s16)))
void __arm_vstrhq_s16(int16_t *, int16x8_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vstrhq_s16)))
void __arm_vstrhq(int16_t *, int16x8_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vstrhq_s32)))
void __arm_vstrhq_s32(int16_t *, int32x4_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vstrhq_s32)))
void __arm_vstrhq(int16_t *, int32x4_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vstrhq_scatter_offset_p_s16)))
void __arm_vstrhq_scatter_offset_p_s16(int16_t *, uint16x8_t, int16x8_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vstrhq_scatter_offset_p_s16)))
void __arm_vstrhq_scatter_offset_p(int16_t *, uint16x8_t, int16x8_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vstrhq_scatter_offset_p_s32)))
void __arm_vstrhq_scatter_offset_p_s32(int16_t *, uint32x4_t, int32x4_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vstrhq_scatter_offset_p_s32)))
void __arm_vstrhq_scatter_offset_p(int16_t *, uint32x4_t, int32x4_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vstrhq_scatter_offset_p_u16)))
void __arm_vstrhq_scatter_offset_p_u16(uint16_t *, uint16x8_t, uint16x8_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vstrhq_scatter_offset_p_u16)))
void __arm_vstrhq_scatter_offset_p(uint16_t *, uint16x8_t, uint16x8_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vstrhq_scatter_offset_p_u32)))
void __arm_vstrhq_scatter_offset_p_u32(uint16_t *, uint32x4_t, uint32x4_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vstrhq_scatter_offset_p_u32)))
void __arm_vstrhq_scatter_offset_p(uint16_t *, uint32x4_t, uint32x4_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vstrhq_scatter_offset_s16)))
void __arm_vstrhq_scatter_offset_s16(int16_t *, uint16x8_t, int16x8_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vstrhq_scatter_offset_s16)))
void __arm_vstrhq_scatter_offset(int16_t *, uint16x8_t, int16x8_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vstrhq_scatter_offset_s32)))
void __arm_vstrhq_scatter_offset_s32(int16_t *, uint32x4_t, int32x4_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vstrhq_scatter_offset_s32)))
void __arm_vstrhq_scatter_offset(int16_t *, uint32x4_t, int32x4_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vstrhq_scatter_offset_u16)))
void __arm_vstrhq_scatter_offset_u16(uint16_t *, uint16x8_t, uint16x8_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vstrhq_scatter_offset_u16)))
void __arm_vstrhq_scatter_offset(uint16_t *, uint16x8_t, uint16x8_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vstrhq_scatter_offset_u32)))
void __arm_vstrhq_scatter_offset_u32(uint16_t *, uint32x4_t, uint32x4_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vstrhq_scatter_offset_u32)))
void __arm_vstrhq_scatter_offset(uint16_t *, uint32x4_t, uint32x4_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vstrhq_scatter_shifted_offset_p_s16)))
void __arm_vstrhq_scatter_shifted_offset_p_s16(int16_t *, uint16x8_t, int16x8_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vstrhq_scatter_shifted_offset_p_s16)))
void __arm_vstrhq_scatter_shifted_offset_p(int16_t *, uint16x8_t, int16x8_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vstrhq_scatter_shifted_offset_p_s32)))
void __arm_vstrhq_scatter_shifted_offset_p_s32(int16_t *, uint32x4_t, int32x4_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vstrhq_scatter_shifted_offset_p_s32)))
void __arm_vstrhq_scatter_shifted_offset_p(int16_t *, uint32x4_t, int32x4_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vstrhq_scatter_shifted_offset_p_u16)))
void __arm_vstrhq_scatter_shifted_offset_p_u16(uint16_t *, uint16x8_t, uint16x8_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vstrhq_scatter_shifted_offset_p_u16)))
void __arm_vstrhq_scatter_shifted_offset_p(uint16_t *, uint16x8_t, uint16x8_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vstrhq_scatter_shifted_offset_p_u32)))
void __arm_vstrhq_scatter_shifted_offset_p_u32(uint16_t *, uint32x4_t, uint32x4_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vstrhq_scatter_shifted_offset_p_u32)))
void __arm_vstrhq_scatter_shifted_offset_p(uint16_t *, uint32x4_t, uint32x4_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vstrhq_scatter_shifted_offset_s16)))
void __arm_vstrhq_scatter_shifted_offset_s16(int16_t *, uint16x8_t, int16x8_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vstrhq_scatter_shifted_offset_s16)))
void __arm_vstrhq_scatter_shifted_offset(int16_t *, uint16x8_t, int16x8_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vstrhq_scatter_shifted_offset_s32)))
void __arm_vstrhq_scatter_shifted_offset_s32(int16_t *, uint32x4_t, int32x4_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vstrhq_scatter_shifted_offset_s32)))
void __arm_vstrhq_scatter_shifted_offset(int16_t *, uint32x4_t, int32x4_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vstrhq_scatter_shifted_offset_u16)))
void __arm_vstrhq_scatter_shifted_offset_u16(uint16_t *, uint16x8_t, uint16x8_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vstrhq_scatter_shifted_offset_u16)))
void __arm_vstrhq_scatter_shifted_offset(uint16_t *, uint16x8_t, uint16x8_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vstrhq_scatter_shifted_offset_u32)))
void __arm_vstrhq_scatter_shifted_offset_u32(uint16_t *, uint32x4_t, uint32x4_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vstrhq_scatter_shifted_offset_u32)))
void __arm_vstrhq_scatter_shifted_offset(uint16_t *, uint32x4_t, uint32x4_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vstrhq_u16)))
void __arm_vstrhq_u16(uint16_t *, uint16x8_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vstrhq_u16)))
void __arm_vstrhq(uint16_t *, uint16x8_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vstrhq_u32)))
void __arm_vstrhq_u32(uint16_t *, uint32x4_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vstrhq_u32)))
void __arm_vstrhq(uint16_t *, uint32x4_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vstrwq_p_s32)))
void __arm_vstrwq_p_s32(int32_t *, int32x4_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vstrwq_p_s32)))
void __arm_vstrwq_p(int32_t *, int32x4_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vstrwq_p_u32)))
void __arm_vstrwq_p_u32(uint32_t *, uint32x4_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vstrwq_p_u32)))
void __arm_vstrwq_p(uint32_t *, uint32x4_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vstrwq_s32)))
void __arm_vstrwq_s32(int32_t *, int32x4_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vstrwq_s32)))
void __arm_vstrwq(int32_t *, int32x4_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vstrwq_scatter_base_p_s32)))
void __arm_vstrwq_scatter_base_p_s32(uint32x4_t, int, int32x4_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vstrwq_scatter_base_p_s32)))
void __arm_vstrwq_scatter_base_p(uint32x4_t, int, int32x4_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vstrwq_scatter_base_p_u32)))
void __arm_vstrwq_scatter_base_p_u32(uint32x4_t, int, uint32x4_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vstrwq_scatter_base_p_u32)))
void __arm_vstrwq_scatter_base_p(uint32x4_t, int, uint32x4_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vstrwq_scatter_base_s32)))
void __arm_vstrwq_scatter_base_s32(uint32x4_t, int, int32x4_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vstrwq_scatter_base_s32)))
void __arm_vstrwq_scatter_base(uint32x4_t, int, int32x4_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vstrwq_scatter_base_u32)))
void __arm_vstrwq_scatter_base_u32(uint32x4_t, int, uint32x4_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vstrwq_scatter_base_u32)))
void __arm_vstrwq_scatter_base(uint32x4_t, int, uint32x4_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vstrwq_scatter_base_wb_p_s32)))
void __arm_vstrwq_scatter_base_wb_p_s32(uint32x4_t *, int, int32x4_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vstrwq_scatter_base_wb_p_s32)))
void __arm_vstrwq_scatter_base_wb_p(uint32x4_t *, int, int32x4_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vstrwq_scatter_base_wb_p_u32)))
void __arm_vstrwq_scatter_base_wb_p_u32(uint32x4_t *, int, uint32x4_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vstrwq_scatter_base_wb_p_u32)))
void __arm_vstrwq_scatter_base_wb_p(uint32x4_t *, int, uint32x4_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vstrwq_scatter_base_wb_s32)))
void __arm_vstrwq_scatter_base_wb_s32(uint32x4_t *, int, int32x4_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vstrwq_scatter_base_wb_s32)))
void __arm_vstrwq_scatter_base_wb(uint32x4_t *, int, int32x4_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vstrwq_scatter_base_wb_u32)))
void __arm_vstrwq_scatter_base_wb_u32(uint32x4_t *, int, uint32x4_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vstrwq_scatter_base_wb_u32)))
void __arm_vstrwq_scatter_base_wb(uint32x4_t *, int, uint32x4_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vstrwq_scatter_offset_p_s32)))
void __arm_vstrwq_scatter_offset_p_s32(int32_t *, uint32x4_t, int32x4_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vstrwq_scatter_offset_p_s32)))
void __arm_vstrwq_scatter_offset_p(int32_t *, uint32x4_t, int32x4_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vstrwq_scatter_offset_p_u32)))
void __arm_vstrwq_scatter_offset_p_u32(uint32_t *, uint32x4_t, uint32x4_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vstrwq_scatter_offset_p_u32)))
void __arm_vstrwq_scatter_offset_p(uint32_t *, uint32x4_t, uint32x4_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vstrwq_scatter_offset_s32)))
void __arm_vstrwq_scatter_offset_s32(int32_t *, uint32x4_t, int32x4_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vstrwq_scatter_offset_s32)))
void __arm_vstrwq_scatter_offset(int32_t *, uint32x4_t, int32x4_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vstrwq_scatter_offset_u32)))
void __arm_vstrwq_scatter_offset_u32(uint32_t *, uint32x4_t, uint32x4_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vstrwq_scatter_offset_u32)))
void __arm_vstrwq_scatter_offset(uint32_t *, uint32x4_t, uint32x4_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vstrwq_scatter_shifted_offset_p_s32)))
void __arm_vstrwq_scatter_shifted_offset_p_s32(int32_t *, uint32x4_t, int32x4_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vstrwq_scatter_shifted_offset_p_s32)))
void __arm_vstrwq_scatter_shifted_offset_p(int32_t *, uint32x4_t, int32x4_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vstrwq_scatter_shifted_offset_p_u32)))
void __arm_vstrwq_scatter_shifted_offset_p_u32(uint32_t *, uint32x4_t, uint32x4_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vstrwq_scatter_shifted_offset_p_u32)))
void __arm_vstrwq_scatter_shifted_offset_p(uint32_t *, uint32x4_t, uint32x4_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vstrwq_scatter_shifted_offset_s32)))
void __arm_vstrwq_scatter_shifted_offset_s32(int32_t *, uint32x4_t, int32x4_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vstrwq_scatter_shifted_offset_s32)))
void __arm_vstrwq_scatter_shifted_offset(int32_t *, uint32x4_t, int32x4_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vstrwq_scatter_shifted_offset_u32)))
void __arm_vstrwq_scatter_shifted_offset_u32(uint32_t *, uint32x4_t, uint32x4_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vstrwq_scatter_shifted_offset_u32)))
void __arm_vstrwq_scatter_shifted_offset(uint32_t *, uint32x4_t, uint32x4_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vstrwq_u32)))
void __arm_vstrwq_u32(uint32_t *, uint32x4_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vstrwq_u32)))
void __arm_vstrwq(uint32_t *, uint32x4_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vsubq_m_s16)))
int16x8_t __arm_vsubq_m_s16(int16x8_t, int16x8_t, int16x8_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vsubq_m_s16)))
int16x8_t __arm_vsubq_m(int16x8_t, int16x8_t, int16x8_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vsubq_m_s32)))
int32x4_t __arm_vsubq_m_s32(int32x4_t, int32x4_t, int32x4_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vsubq_m_s32)))
int32x4_t __arm_vsubq_m(int32x4_t, int32x4_t, int32x4_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vsubq_m_s8)))
int8x16_t __arm_vsubq_m_s8(int8x16_t, int8x16_t, int8x16_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vsubq_m_s8)))
int8x16_t __arm_vsubq_m(int8x16_t, int8x16_t, int8x16_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vsubq_m_u16)))
uint16x8_t __arm_vsubq_m_u16(uint16x8_t, uint16x8_t, uint16x8_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vsubq_m_u16)))
uint16x8_t __arm_vsubq_m(uint16x8_t, uint16x8_t, uint16x8_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vsubq_m_u32)))
uint32x4_t __arm_vsubq_m_u32(uint32x4_t, uint32x4_t, uint32x4_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vsubq_m_u32)))
uint32x4_t __arm_vsubq_m(uint32x4_t, uint32x4_t, uint32x4_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vsubq_m_u8)))
uint8x16_t __arm_vsubq_m_u8(uint8x16_t, uint8x16_t, uint8x16_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vsubq_m_u8)))
uint8x16_t __arm_vsubq_m(uint8x16_t, uint8x16_t, uint8x16_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vsubq_s16)))
int16x8_t __arm_vsubq_s16(int16x8_t, int16x8_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vsubq_s16)))
int16x8_t __arm_vsubq(int16x8_t, int16x8_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vsubq_s32)))
int32x4_t __arm_vsubq_s32(int32x4_t, int32x4_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vsubq_s32)))
int32x4_t __arm_vsubq(int32x4_t, int32x4_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vsubq_s8)))
int8x16_t __arm_vsubq_s8(int8x16_t, int8x16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vsubq_s8)))
int8x16_t __arm_vsubq(int8x16_t, int8x16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vsubq_u16)))
uint16x8_t __arm_vsubq_u16(uint16x8_t, uint16x8_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vsubq_u16)))
uint16x8_t __arm_vsubq(uint16x8_t, uint16x8_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vsubq_u32)))
uint32x4_t __arm_vsubq_u32(uint32x4_t, uint32x4_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vsubq_u32)))
uint32x4_t __arm_vsubq(uint32x4_t, uint32x4_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vsubq_u8)))
uint8x16_t __arm_vsubq_u8(uint8x16_t, uint8x16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vsubq_u8)))
uint8x16_t __arm_vsubq(uint8x16_t, uint8x16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vuninitializedq_polymorphic_s16)))
int16x8_t __arm_vuninitializedq(int16x8_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vuninitializedq_polymorphic_s32)))
int32x4_t __arm_vuninitializedq(int32x4_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vuninitializedq_polymorphic_s64)))
int64x2_t __arm_vuninitializedq(int64x2_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vuninitializedq_polymorphic_s8)))
int8x16_t __arm_vuninitializedq(int8x16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vuninitializedq_polymorphic_u16)))
uint16x8_t __arm_vuninitializedq(uint16x8_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vuninitializedq_polymorphic_u32)))
uint32x4_t __arm_vuninitializedq(uint32x4_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vuninitializedq_polymorphic_u64)))
uint64x2_t __arm_vuninitializedq(uint64x2_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vuninitializedq_polymorphic_u8)))
uint8x16_t __arm_vuninitializedq(uint8x16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vuninitializedq_s16)))
int16x8_t __arm_vuninitializedq_s16();
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vuninitializedq_s32)))
int32x4_t __arm_vuninitializedq_s32();
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vuninitializedq_s64)))
int64x2_t __arm_vuninitializedq_s64();
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vuninitializedq_s8)))
int8x16_t __arm_vuninitializedq_s8();
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vuninitializedq_u16)))
uint16x8_t __arm_vuninitializedq_u16();
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vuninitializedq_u32)))
uint32x4_t __arm_vuninitializedq_u32();
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vuninitializedq_u64)))
uint64x2_t __arm_vuninitializedq_u64();
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vuninitializedq_u8)))
uint8x16_t __arm_vuninitializedq_u8();

#if (__ARM_FEATURE_MVE & 2)

typedef __fp16 float16_t;
typedef float float32_t;
typedef __attribute__((neon_vector_type(8))) float16_t float16x8_t;
typedef struct { float16x8_t val[2]; } float16x8x2_t;
typedef struct { float16x8_t val[4]; } float16x8x4_t;
typedef __attribute__((neon_vector_type(4))) float32_t float32x4_t;
typedef struct { float32x4_t val[2]; } float32x4x2_t;
typedef struct { float32x4_t val[4]; } float32x4x4_t;

static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vabdq_f16)))
float16x8_t __arm_vabdq_f16(float16x8_t, float16x8_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vabdq_f16)))
float16x8_t __arm_vabdq(float16x8_t, float16x8_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vabdq_f32)))
float32x4_t __arm_vabdq_f32(float32x4_t, float32x4_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vabdq_f32)))
float32x4_t __arm_vabdq(float32x4_t, float32x4_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vabdq_m_f16)))
float16x8_t __arm_vabdq_m_f16(float16x8_t, float16x8_t, float16x8_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vabdq_m_f16)))
float16x8_t __arm_vabdq_m(float16x8_t, float16x8_t, float16x8_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vabdq_m_f32)))
float32x4_t __arm_vabdq_m_f32(float32x4_t, float32x4_t, float32x4_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vabdq_m_f32)))
float32x4_t __arm_vabdq_m(float32x4_t, float32x4_t, float32x4_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vaddq_f16)))
float16x8_t __arm_vaddq_f16(float16x8_t, float16x8_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vaddq_f16)))
float16x8_t __arm_vaddq(float16x8_t, float16x8_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vaddq_f32)))
float32x4_t __arm_vaddq_f32(float32x4_t, float32x4_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vaddq_f32)))
float32x4_t __arm_vaddq(float32x4_t, float32x4_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vaddq_m_f16)))
float16x8_t __arm_vaddq_m_f16(float16x8_t, float16x8_t, float16x8_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vaddq_m_f16)))
float16x8_t __arm_vaddq_m(float16x8_t, float16x8_t, float16x8_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vaddq_m_f32)))
float32x4_t __arm_vaddq_m_f32(float32x4_t, float32x4_t, float32x4_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vaddq_m_f32)))
float32x4_t __arm_vaddq_m(float32x4_t, float32x4_t, float32x4_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vandq_f16)))
float16x8_t __arm_vandq_f16(float16x8_t, float16x8_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vandq_f16)))
float16x8_t __arm_vandq(float16x8_t, float16x8_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vandq_f32)))
float32x4_t __arm_vandq_f32(float32x4_t, float32x4_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vandq_f32)))
float32x4_t __arm_vandq(float32x4_t, float32x4_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vandq_m_f16)))
float16x8_t __arm_vandq_m_f16(float16x8_t, float16x8_t, float16x8_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vandq_m_f16)))
float16x8_t __arm_vandq_m(float16x8_t, float16x8_t, float16x8_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vandq_m_f32)))
float32x4_t __arm_vandq_m_f32(float32x4_t, float32x4_t, float32x4_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vandq_m_f32)))
float32x4_t __arm_vandq_m(float32x4_t, float32x4_t, float32x4_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vbicq_f16)))
float16x8_t __arm_vbicq_f16(float16x8_t, float16x8_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vbicq_f16)))
float16x8_t __arm_vbicq(float16x8_t, float16x8_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vbicq_f32)))
float32x4_t __arm_vbicq_f32(float32x4_t, float32x4_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vbicq_f32)))
float32x4_t __arm_vbicq(float32x4_t, float32x4_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vbicq_m_f16)))
float16x8_t __arm_vbicq_m_f16(float16x8_t, float16x8_t, float16x8_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vbicq_m_f16)))
float16x8_t __arm_vbicq_m(float16x8_t, float16x8_t, float16x8_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vbicq_m_f32)))
float32x4_t __arm_vbicq_m_f32(float32x4_t, float32x4_t, float32x4_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vbicq_m_f32)))
float32x4_t __arm_vbicq_m(float32x4_t, float32x4_t, float32x4_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpeqq_f16)))
mve_pred16_t __arm_vcmpeqq_f16(float16x8_t, float16x8_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpeqq_f16)))
mve_pred16_t __arm_vcmpeqq(float16x8_t, float16x8_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpeqq_f32)))
mve_pred16_t __arm_vcmpeqq_f32(float32x4_t, float32x4_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpeqq_f32)))
mve_pred16_t __arm_vcmpeqq(float32x4_t, float32x4_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpeqq_m_f16)))
mve_pred16_t __arm_vcmpeqq_m_f16(float16x8_t, float16x8_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpeqq_m_f16)))
mve_pred16_t __arm_vcmpeqq_m(float16x8_t, float16x8_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpeqq_m_f32)))
mve_pred16_t __arm_vcmpeqq_m_f32(float32x4_t, float32x4_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpeqq_m_f32)))
mve_pred16_t __arm_vcmpeqq_m(float32x4_t, float32x4_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpeqq_m_n_f16)))
mve_pred16_t __arm_vcmpeqq_m_n_f16(float16x8_t, float16_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpeqq_m_n_f16)))
mve_pred16_t __arm_vcmpeqq_m(float16x8_t, float16_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpeqq_m_n_f32)))
mve_pred16_t __arm_vcmpeqq_m_n_f32(float32x4_t, float32_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpeqq_m_n_f32)))
mve_pred16_t __arm_vcmpeqq_m(float32x4_t, float32_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpeqq_n_f16)))
mve_pred16_t __arm_vcmpeqq_n_f16(float16x8_t, float16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpeqq_n_f16)))
mve_pred16_t __arm_vcmpeqq(float16x8_t, float16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpeqq_n_f32)))
mve_pred16_t __arm_vcmpeqq_n_f32(float32x4_t, float32_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpeqq_n_f32)))
mve_pred16_t __arm_vcmpeqq(float32x4_t, float32_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpgeq_f16)))
mve_pred16_t __arm_vcmpgeq_f16(float16x8_t, float16x8_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpgeq_f16)))
mve_pred16_t __arm_vcmpgeq(float16x8_t, float16x8_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpgeq_f32)))
mve_pred16_t __arm_vcmpgeq_f32(float32x4_t, float32x4_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpgeq_f32)))
mve_pred16_t __arm_vcmpgeq(float32x4_t, float32x4_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpgeq_m_f16)))
mve_pred16_t __arm_vcmpgeq_m_f16(float16x8_t, float16x8_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpgeq_m_f16)))
mve_pred16_t __arm_vcmpgeq_m(float16x8_t, float16x8_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpgeq_m_f32)))
mve_pred16_t __arm_vcmpgeq_m_f32(float32x4_t, float32x4_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpgeq_m_f32)))
mve_pred16_t __arm_vcmpgeq_m(float32x4_t, float32x4_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpgeq_m_n_f16)))
mve_pred16_t __arm_vcmpgeq_m_n_f16(float16x8_t, float16_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpgeq_m_n_f16)))
mve_pred16_t __arm_vcmpgeq_m(float16x8_t, float16_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpgeq_m_n_f32)))
mve_pred16_t __arm_vcmpgeq_m_n_f32(float32x4_t, float32_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpgeq_m_n_f32)))
mve_pred16_t __arm_vcmpgeq_m(float32x4_t, float32_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpgeq_n_f16)))
mve_pred16_t __arm_vcmpgeq_n_f16(float16x8_t, float16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpgeq_n_f16)))
mve_pred16_t __arm_vcmpgeq(float16x8_t, float16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpgeq_n_f32)))
mve_pred16_t __arm_vcmpgeq_n_f32(float32x4_t, float32_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpgeq_n_f32)))
mve_pred16_t __arm_vcmpgeq(float32x4_t, float32_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpgtq_f16)))
mve_pred16_t __arm_vcmpgtq_f16(float16x8_t, float16x8_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpgtq_f16)))
mve_pred16_t __arm_vcmpgtq(float16x8_t, float16x8_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpgtq_f32)))
mve_pred16_t __arm_vcmpgtq_f32(float32x4_t, float32x4_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpgtq_f32)))
mve_pred16_t __arm_vcmpgtq(float32x4_t, float32x4_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpgtq_m_f16)))
mve_pred16_t __arm_vcmpgtq_m_f16(float16x8_t, float16x8_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpgtq_m_f16)))
mve_pred16_t __arm_vcmpgtq_m(float16x8_t, float16x8_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpgtq_m_f32)))
mve_pred16_t __arm_vcmpgtq_m_f32(float32x4_t, float32x4_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpgtq_m_f32)))
mve_pred16_t __arm_vcmpgtq_m(float32x4_t, float32x4_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpgtq_m_n_f16)))
mve_pred16_t __arm_vcmpgtq_m_n_f16(float16x8_t, float16_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpgtq_m_n_f16)))
mve_pred16_t __arm_vcmpgtq_m(float16x8_t, float16_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpgtq_m_n_f32)))
mve_pred16_t __arm_vcmpgtq_m_n_f32(float32x4_t, float32_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpgtq_m_n_f32)))
mve_pred16_t __arm_vcmpgtq_m(float32x4_t, float32_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpgtq_n_f16)))
mve_pred16_t __arm_vcmpgtq_n_f16(float16x8_t, float16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpgtq_n_f16)))
mve_pred16_t __arm_vcmpgtq(float16x8_t, float16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpgtq_n_f32)))
mve_pred16_t __arm_vcmpgtq_n_f32(float32x4_t, float32_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpgtq_n_f32)))
mve_pred16_t __arm_vcmpgtq(float32x4_t, float32_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpleq_f16)))
mve_pred16_t __arm_vcmpleq_f16(float16x8_t, float16x8_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpleq_f16)))
mve_pred16_t __arm_vcmpleq(float16x8_t, float16x8_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpleq_f32)))
mve_pred16_t __arm_vcmpleq_f32(float32x4_t, float32x4_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpleq_f32)))
mve_pred16_t __arm_vcmpleq(float32x4_t, float32x4_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpleq_m_f16)))
mve_pred16_t __arm_vcmpleq_m_f16(float16x8_t, float16x8_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpleq_m_f16)))
mve_pred16_t __arm_vcmpleq_m(float16x8_t, float16x8_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpleq_m_f32)))
mve_pred16_t __arm_vcmpleq_m_f32(float32x4_t, float32x4_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpleq_m_f32)))
mve_pred16_t __arm_vcmpleq_m(float32x4_t, float32x4_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpleq_m_n_f16)))
mve_pred16_t __arm_vcmpleq_m_n_f16(float16x8_t, float16_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpleq_m_n_f16)))
mve_pred16_t __arm_vcmpleq_m(float16x8_t, float16_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpleq_m_n_f32)))
mve_pred16_t __arm_vcmpleq_m_n_f32(float32x4_t, float32_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpleq_m_n_f32)))
mve_pred16_t __arm_vcmpleq_m(float32x4_t, float32_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpleq_n_f16)))
mve_pred16_t __arm_vcmpleq_n_f16(float16x8_t, float16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpleq_n_f16)))
mve_pred16_t __arm_vcmpleq(float16x8_t, float16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpleq_n_f32)))
mve_pred16_t __arm_vcmpleq_n_f32(float32x4_t, float32_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpleq_n_f32)))
mve_pred16_t __arm_vcmpleq(float32x4_t, float32_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpltq_f16)))
mve_pred16_t __arm_vcmpltq_f16(float16x8_t, float16x8_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpltq_f16)))
mve_pred16_t __arm_vcmpltq(float16x8_t, float16x8_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpltq_f32)))
mve_pred16_t __arm_vcmpltq_f32(float32x4_t, float32x4_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpltq_f32)))
mve_pred16_t __arm_vcmpltq(float32x4_t, float32x4_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpltq_m_f16)))
mve_pred16_t __arm_vcmpltq_m_f16(float16x8_t, float16x8_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpltq_m_f16)))
mve_pred16_t __arm_vcmpltq_m(float16x8_t, float16x8_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpltq_m_f32)))
mve_pred16_t __arm_vcmpltq_m_f32(float32x4_t, float32x4_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpltq_m_f32)))
mve_pred16_t __arm_vcmpltq_m(float32x4_t, float32x4_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpltq_m_n_f16)))
mve_pred16_t __arm_vcmpltq_m_n_f16(float16x8_t, float16_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpltq_m_n_f16)))
mve_pred16_t __arm_vcmpltq_m(float16x8_t, float16_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpltq_m_n_f32)))
mve_pred16_t __arm_vcmpltq_m_n_f32(float32x4_t, float32_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpltq_m_n_f32)))
mve_pred16_t __arm_vcmpltq_m(float32x4_t, float32_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpltq_n_f16)))
mve_pred16_t __arm_vcmpltq_n_f16(float16x8_t, float16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpltq_n_f16)))
mve_pred16_t __arm_vcmpltq(float16x8_t, float16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpltq_n_f32)))
mve_pred16_t __arm_vcmpltq_n_f32(float32x4_t, float32_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpltq_n_f32)))
mve_pred16_t __arm_vcmpltq(float32x4_t, float32_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpneq_f16)))
mve_pred16_t __arm_vcmpneq_f16(float16x8_t, float16x8_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpneq_f16)))
mve_pred16_t __arm_vcmpneq(float16x8_t, float16x8_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpneq_f32)))
mve_pred16_t __arm_vcmpneq_f32(float32x4_t, float32x4_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpneq_f32)))
mve_pred16_t __arm_vcmpneq(float32x4_t, float32x4_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpneq_m_f16)))
mve_pred16_t __arm_vcmpneq_m_f16(float16x8_t, float16x8_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpneq_m_f16)))
mve_pred16_t __arm_vcmpneq_m(float16x8_t, float16x8_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpneq_m_f32)))
mve_pred16_t __arm_vcmpneq_m_f32(float32x4_t, float32x4_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpneq_m_f32)))
mve_pred16_t __arm_vcmpneq_m(float32x4_t, float32x4_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpneq_m_n_f16)))
mve_pred16_t __arm_vcmpneq_m_n_f16(float16x8_t, float16_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpneq_m_n_f16)))
mve_pred16_t __arm_vcmpneq_m(float16x8_t, float16_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpneq_m_n_f32)))
mve_pred16_t __arm_vcmpneq_m_n_f32(float32x4_t, float32_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpneq_m_n_f32)))
mve_pred16_t __arm_vcmpneq_m(float32x4_t, float32_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpneq_n_f16)))
mve_pred16_t __arm_vcmpneq_n_f16(float16x8_t, float16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpneq_n_f16)))
mve_pred16_t __arm_vcmpneq(float16x8_t, float16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpneq_n_f32)))
mve_pred16_t __arm_vcmpneq_n_f32(float32x4_t, float32_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpneq_n_f32)))
mve_pred16_t __arm_vcmpneq(float32x4_t, float32_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcreateq_f16)))
float16x8_t __arm_vcreateq_f16(uint64_t, uint64_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcreateq_f32)))
float32x4_t __arm_vcreateq_f32(uint64_t, uint64_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcvtbq_f16_f32)))
float16x8_t __arm_vcvtbq_f16_f32(float16x8_t, float32x4_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcvtbq_m_f16_f32)))
float16x8_t __arm_vcvtbq_m_f16_f32(float16x8_t, float32x4_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcvttq_f16_f32)))
float16x8_t __arm_vcvttq_f16_f32(float16x8_t, float32x4_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcvttq_m_f16_f32)))
float16x8_t __arm_vcvttq_m_f16_f32(float16x8_t, float32x4_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_veorq_f16)))
float16x8_t __arm_veorq_f16(float16x8_t, float16x8_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_veorq_f16)))
float16x8_t __arm_veorq(float16x8_t, float16x8_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_veorq_f32)))
float32x4_t __arm_veorq_f32(float32x4_t, float32x4_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_veorq_f32)))
float32x4_t __arm_veorq(float32x4_t, float32x4_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_veorq_m_f16)))
float16x8_t __arm_veorq_m_f16(float16x8_t, float16x8_t, float16x8_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_veorq_m_f16)))
float16x8_t __arm_veorq_m(float16x8_t, float16x8_t, float16x8_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_veorq_m_f32)))
float32x4_t __arm_veorq_m_f32(float32x4_t, float32x4_t, float32x4_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_veorq_m_f32)))
float32x4_t __arm_veorq_m(float32x4_t, float32x4_t, float32x4_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vgetq_lane_f16)))
float16_t __arm_vgetq_lane_f16(float16x8_t, int);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vgetq_lane_f16)))
float16_t __arm_vgetq_lane(float16x8_t, int);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vgetq_lane_f32)))
float32_t __arm_vgetq_lane_f32(float32x4_t, int);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vgetq_lane_f32)))
float32_t __arm_vgetq_lane(float32x4_t, int);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vld1q_f16)))
float16x8_t __arm_vld1q_f16(const float16_t *);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vld1q_f16)))
float16x8_t __arm_vld1q(const float16_t *);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vld1q_f32)))
float32x4_t __arm_vld1q_f32(const float32_t *);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vld1q_f32)))
float32x4_t __arm_vld1q(const float32_t *);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vld1q_z_f16)))
float16x8_t __arm_vld1q_z_f16(const float16_t *, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vld1q_z_f16)))
float16x8_t __arm_vld1q_z(const float16_t *, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vld1q_z_f32)))
float32x4_t __arm_vld1q_z_f32(const float32_t *, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vld1q_z_f32)))
float32x4_t __arm_vld1q_z(const float32_t *, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vld2q_f16)))
float16x8x2_t __arm_vld2q_f16(const float16_t *);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vld2q_f16)))
float16x8x2_t __arm_vld2q(const float16_t *);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vld2q_f32)))
float32x4x2_t __arm_vld2q_f32(const float32_t *);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vld2q_f32)))
float32x4x2_t __arm_vld2q(const float32_t *);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vld4q_f16)))
float16x8x4_t __arm_vld4q_f16(const float16_t *);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vld4q_f16)))
float16x8x4_t __arm_vld4q(const float16_t *);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vld4q_f32)))
float32x4x4_t __arm_vld4q_f32(const float32_t *);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vld4q_f32)))
float32x4x4_t __arm_vld4q(const float32_t *);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vldrhq_f16)))
float16x8_t __arm_vldrhq_f16(const float16_t *);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vldrhq_gather_offset_f16)))
float16x8_t __arm_vldrhq_gather_offset_f16(const float16_t *, uint16x8_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vldrhq_gather_offset_f16)))
float16x8_t __arm_vldrhq_gather_offset(const float16_t *, uint16x8_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vldrhq_gather_offset_z_f16)))
float16x8_t __arm_vldrhq_gather_offset_z_f16(const float16_t *, uint16x8_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vldrhq_gather_offset_z_f16)))
float16x8_t __arm_vldrhq_gather_offset_z(const float16_t *, uint16x8_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vldrhq_gather_shifted_offset_f16)))
float16x8_t __arm_vldrhq_gather_shifted_offset_f16(const float16_t *, uint16x8_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vldrhq_gather_shifted_offset_f16)))
float16x8_t __arm_vldrhq_gather_shifted_offset(const float16_t *, uint16x8_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vldrhq_gather_shifted_offset_z_f16)))
float16x8_t __arm_vldrhq_gather_shifted_offset_z_f16(const float16_t *, uint16x8_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vldrhq_gather_shifted_offset_z_f16)))
float16x8_t __arm_vldrhq_gather_shifted_offset_z(const float16_t *, uint16x8_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vldrhq_z_f16)))
float16x8_t __arm_vldrhq_z_f16(const float16_t *, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vldrwq_f32)))
float32x4_t __arm_vldrwq_f32(const float32_t *);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vldrwq_gather_base_f32)))
float32x4_t __arm_vldrwq_gather_base_f32(uint32x4_t, int);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vldrwq_gather_base_wb_f32)))
float32x4_t __arm_vldrwq_gather_base_wb_f32(uint32x4_t *, int);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vldrwq_gather_base_wb_z_f32)))
float32x4_t __arm_vldrwq_gather_base_wb_z_f32(uint32x4_t *, int, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vldrwq_gather_base_z_f32)))
float32x4_t __arm_vldrwq_gather_base_z_f32(uint32x4_t, int, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vldrwq_gather_offset_f32)))
float32x4_t __arm_vldrwq_gather_offset_f32(const float32_t *, uint32x4_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vldrwq_gather_offset_f32)))
float32x4_t __arm_vldrwq_gather_offset(const float32_t *, uint32x4_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vldrwq_gather_offset_z_f32)))
float32x4_t __arm_vldrwq_gather_offset_z_f32(const float32_t *, uint32x4_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vldrwq_gather_offset_z_f32)))
float32x4_t __arm_vldrwq_gather_offset_z(const float32_t *, uint32x4_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vldrwq_gather_shifted_offset_f32)))
float32x4_t __arm_vldrwq_gather_shifted_offset_f32(const float32_t *, uint32x4_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vldrwq_gather_shifted_offset_f32)))
float32x4_t __arm_vldrwq_gather_shifted_offset(const float32_t *, uint32x4_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vldrwq_gather_shifted_offset_z_f32)))
float32x4_t __arm_vldrwq_gather_shifted_offset_z_f32(const float32_t *, uint32x4_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vldrwq_gather_shifted_offset_z_f32)))
float32x4_t __arm_vldrwq_gather_shifted_offset_z(const float32_t *, uint32x4_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vldrwq_z_f32)))
float32x4_t __arm_vldrwq_z_f32(const float32_t *, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vmulq_f16)))
float16x8_t __arm_vmulq_f16(float16x8_t, float16x8_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vmulq_f16)))
float16x8_t __arm_vmulq(float16x8_t, float16x8_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vmulq_f32)))
float32x4_t __arm_vmulq_f32(float32x4_t, float32x4_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vmulq_f32)))
float32x4_t __arm_vmulq(float32x4_t, float32x4_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vmulq_m_f16)))
float16x8_t __arm_vmulq_m_f16(float16x8_t, float16x8_t, float16x8_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vmulq_m_f16)))
float16x8_t __arm_vmulq_m(float16x8_t, float16x8_t, float16x8_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vmulq_m_f32)))
float32x4_t __arm_vmulq_m_f32(float32x4_t, float32x4_t, float32x4_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vmulq_m_f32)))
float32x4_t __arm_vmulq_m(float32x4_t, float32x4_t, float32x4_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vornq_f16)))
float16x8_t __arm_vornq_f16(float16x8_t, float16x8_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vornq_f16)))
float16x8_t __arm_vornq(float16x8_t, float16x8_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vornq_f32)))
float32x4_t __arm_vornq_f32(float32x4_t, float32x4_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vornq_f32)))
float32x4_t __arm_vornq(float32x4_t, float32x4_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vornq_m_f16)))
float16x8_t __arm_vornq_m_f16(float16x8_t, float16x8_t, float16x8_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vornq_m_f16)))
float16x8_t __arm_vornq_m(float16x8_t, float16x8_t, float16x8_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vornq_m_f32)))
float32x4_t __arm_vornq_m_f32(float32x4_t, float32x4_t, float32x4_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vornq_m_f32)))
float32x4_t __arm_vornq_m(float32x4_t, float32x4_t, float32x4_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vorrq_f16)))
float16x8_t __arm_vorrq_f16(float16x8_t, float16x8_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vorrq_f16)))
float16x8_t __arm_vorrq(float16x8_t, float16x8_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vorrq_f32)))
float32x4_t __arm_vorrq_f32(float32x4_t, float32x4_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vorrq_f32)))
float32x4_t __arm_vorrq(float32x4_t, float32x4_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vorrq_m_f16)))
float16x8_t __arm_vorrq_m_f16(float16x8_t, float16x8_t, float16x8_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vorrq_m_f16)))
float16x8_t __arm_vorrq_m(float16x8_t, float16x8_t, float16x8_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vorrq_m_f32)))
float32x4_t __arm_vorrq_m_f32(float32x4_t, float32x4_t, float32x4_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vorrq_m_f32)))
float32x4_t __arm_vorrq_m(float32x4_t, float32x4_t, float32x4_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_f16_f32)))
float16x8_t __arm_vreinterpretq_f16_f32(float32x4_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_f16_f32)))
float16x8_t __arm_vreinterpretq_f16(float32x4_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_f16_s16)))
float16x8_t __arm_vreinterpretq_f16_s16(int16x8_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_f16_s16)))
float16x8_t __arm_vreinterpretq_f16(int16x8_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_f16_s32)))
float16x8_t __arm_vreinterpretq_f16_s32(int32x4_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_f16_s32)))
float16x8_t __arm_vreinterpretq_f16(int32x4_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_f16_s64)))
float16x8_t __arm_vreinterpretq_f16_s64(int64x2_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_f16_s64)))
float16x8_t __arm_vreinterpretq_f16(int64x2_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_f16_s8)))
float16x8_t __arm_vreinterpretq_f16_s8(int8x16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_f16_s8)))
float16x8_t __arm_vreinterpretq_f16(int8x16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_f16_u16)))
float16x8_t __arm_vreinterpretq_f16_u16(uint16x8_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_f16_u16)))
float16x8_t __arm_vreinterpretq_f16(uint16x8_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_f16_u32)))
float16x8_t __arm_vreinterpretq_f16_u32(uint32x4_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_f16_u32)))
float16x8_t __arm_vreinterpretq_f16(uint32x4_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_f16_u64)))
float16x8_t __arm_vreinterpretq_f16_u64(uint64x2_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_f16_u64)))
float16x8_t __arm_vreinterpretq_f16(uint64x2_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_f16_u8)))
float16x8_t __arm_vreinterpretq_f16_u8(uint8x16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_f16_u8)))
float16x8_t __arm_vreinterpretq_f16(uint8x16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_f32_f16)))
float32x4_t __arm_vreinterpretq_f32_f16(float16x8_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_f32_f16)))
float32x4_t __arm_vreinterpretq_f32(float16x8_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_f32_s16)))
float32x4_t __arm_vreinterpretq_f32_s16(int16x8_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_f32_s16)))
float32x4_t __arm_vreinterpretq_f32(int16x8_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_f32_s32)))
float32x4_t __arm_vreinterpretq_f32_s32(int32x4_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_f32_s32)))
float32x4_t __arm_vreinterpretq_f32(int32x4_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_f32_s64)))
float32x4_t __arm_vreinterpretq_f32_s64(int64x2_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_f32_s64)))
float32x4_t __arm_vreinterpretq_f32(int64x2_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_f32_s8)))
float32x4_t __arm_vreinterpretq_f32_s8(int8x16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_f32_s8)))
float32x4_t __arm_vreinterpretq_f32(int8x16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_f32_u16)))
float32x4_t __arm_vreinterpretq_f32_u16(uint16x8_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_f32_u16)))
float32x4_t __arm_vreinterpretq_f32(uint16x8_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_f32_u32)))
float32x4_t __arm_vreinterpretq_f32_u32(uint32x4_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_f32_u32)))
float32x4_t __arm_vreinterpretq_f32(uint32x4_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_f32_u64)))
float32x4_t __arm_vreinterpretq_f32_u64(uint64x2_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_f32_u64)))
float32x4_t __arm_vreinterpretq_f32(uint64x2_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_f32_u8)))
float32x4_t __arm_vreinterpretq_f32_u8(uint8x16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_f32_u8)))
float32x4_t __arm_vreinterpretq_f32(uint8x16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_s16_f16)))
int16x8_t __arm_vreinterpretq_s16_f16(float16x8_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_s16_f16)))
int16x8_t __arm_vreinterpretq_s16(float16x8_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_s16_f32)))
int16x8_t __arm_vreinterpretq_s16_f32(float32x4_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_s16_f32)))
int16x8_t __arm_vreinterpretq_s16(float32x4_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_s32_f16)))
int32x4_t __arm_vreinterpretq_s32_f16(float16x8_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_s32_f16)))
int32x4_t __arm_vreinterpretq_s32(float16x8_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_s32_f32)))
int32x4_t __arm_vreinterpretq_s32_f32(float32x4_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_s32_f32)))
int32x4_t __arm_vreinterpretq_s32(float32x4_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_s64_f16)))
int64x2_t __arm_vreinterpretq_s64_f16(float16x8_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_s64_f16)))
int64x2_t __arm_vreinterpretq_s64(float16x8_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_s64_f32)))
int64x2_t __arm_vreinterpretq_s64_f32(float32x4_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_s64_f32)))
int64x2_t __arm_vreinterpretq_s64(float32x4_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_s8_f16)))
int8x16_t __arm_vreinterpretq_s8_f16(float16x8_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_s8_f16)))
int8x16_t __arm_vreinterpretq_s8(float16x8_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_s8_f32)))
int8x16_t __arm_vreinterpretq_s8_f32(float32x4_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_s8_f32)))
int8x16_t __arm_vreinterpretq_s8(float32x4_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_u16_f16)))
uint16x8_t __arm_vreinterpretq_u16_f16(float16x8_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_u16_f16)))
uint16x8_t __arm_vreinterpretq_u16(float16x8_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_u16_f32)))
uint16x8_t __arm_vreinterpretq_u16_f32(float32x4_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_u16_f32)))
uint16x8_t __arm_vreinterpretq_u16(float32x4_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_u32_f16)))
uint32x4_t __arm_vreinterpretq_u32_f16(float16x8_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_u32_f16)))
uint32x4_t __arm_vreinterpretq_u32(float16x8_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_u32_f32)))
uint32x4_t __arm_vreinterpretq_u32_f32(float32x4_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_u32_f32)))
uint32x4_t __arm_vreinterpretq_u32(float32x4_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_u64_f16)))
uint64x2_t __arm_vreinterpretq_u64_f16(float16x8_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_u64_f16)))
uint64x2_t __arm_vreinterpretq_u64(float16x8_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_u64_f32)))
uint64x2_t __arm_vreinterpretq_u64_f32(float32x4_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_u64_f32)))
uint64x2_t __arm_vreinterpretq_u64(float32x4_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_u8_f16)))
uint8x16_t __arm_vreinterpretq_u8_f16(float16x8_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_u8_f16)))
uint8x16_t __arm_vreinterpretq_u8(float16x8_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_u8_f32)))
uint8x16_t __arm_vreinterpretq_u8_f32(float32x4_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_u8_f32)))
uint8x16_t __arm_vreinterpretq_u8(float32x4_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vsetq_lane_f16)))
float16x8_t __arm_vsetq_lane_f16(float16_t, float16x8_t, int);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vsetq_lane_f16)))
float16x8_t __arm_vsetq_lane(float16_t, float16x8_t, int);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vsetq_lane_f32)))
float32x4_t __arm_vsetq_lane_f32(float32_t, float32x4_t, int);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vsetq_lane_f32)))
float32x4_t __arm_vsetq_lane(float32_t, float32x4_t, int);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vst1q_f16)))
void __arm_vst1q_f16(float16_t *, float16x8_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vst1q_f16)))
void __arm_vst1q(float16_t *, float16x8_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vst1q_f32)))
void __arm_vst1q_f32(float32_t *, float32x4_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vst1q_f32)))
void __arm_vst1q(float32_t *, float32x4_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vst1q_p_f16)))
void __arm_vst1q_p_f16(float16_t *, float16x8_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vst1q_p_f16)))
void __arm_vst1q_p(float16_t *, float16x8_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vst1q_p_f32)))
void __arm_vst1q_p_f32(float32_t *, float32x4_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vst1q_p_f32)))
void __arm_vst1q_p(float32_t *, float32x4_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vst2q_f16)))
void __arm_vst2q_f16(float16_t *, float16x8x2_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vst2q_f16)))
void __arm_vst2q(float16_t *, float16x8x2_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vst2q_f32)))
void __arm_vst2q_f32(float32_t *, float32x4x2_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vst2q_f32)))
void __arm_vst2q(float32_t *, float32x4x2_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vst4q_f16)))
void __arm_vst4q_f16(float16_t *, float16x8x4_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vst4q_f16)))
void __arm_vst4q(float16_t *, float16x8x4_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vst4q_f32)))
void __arm_vst4q_f32(float32_t *, float32x4x4_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vst4q_f32)))
void __arm_vst4q(float32_t *, float32x4x4_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vstrhq_f16)))
void __arm_vstrhq_f16(float16_t *, float16x8_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vstrhq_f16)))
void __arm_vstrhq(float16_t *, float16x8_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vstrhq_p_f16)))
void __arm_vstrhq_p_f16(float16_t *, float16x8_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vstrhq_p_f16)))
void __arm_vstrhq_p(float16_t *, float16x8_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vstrhq_scatter_offset_f16)))
void __arm_vstrhq_scatter_offset_f16(float16_t *, uint16x8_t, float16x8_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vstrhq_scatter_offset_f16)))
void __arm_vstrhq_scatter_offset(float16_t *, uint16x8_t, float16x8_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vstrhq_scatter_offset_p_f16)))
void __arm_vstrhq_scatter_offset_p_f16(float16_t *, uint16x8_t, float16x8_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vstrhq_scatter_offset_p_f16)))
void __arm_vstrhq_scatter_offset_p(float16_t *, uint16x8_t, float16x8_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vstrhq_scatter_shifted_offset_f16)))
void __arm_vstrhq_scatter_shifted_offset_f16(float16_t *, uint16x8_t, float16x8_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vstrhq_scatter_shifted_offset_f16)))
void __arm_vstrhq_scatter_shifted_offset(float16_t *, uint16x8_t, float16x8_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vstrhq_scatter_shifted_offset_p_f16)))
void __arm_vstrhq_scatter_shifted_offset_p_f16(float16_t *, uint16x8_t, float16x8_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vstrhq_scatter_shifted_offset_p_f16)))
void __arm_vstrhq_scatter_shifted_offset_p(float16_t *, uint16x8_t, float16x8_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vstrwq_f32)))
void __arm_vstrwq_f32(float32_t *, float32x4_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vstrwq_f32)))
void __arm_vstrwq(float32_t *, float32x4_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vstrwq_p_f32)))
void __arm_vstrwq_p_f32(float32_t *, float32x4_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vstrwq_p_f32)))
void __arm_vstrwq_p(float32_t *, float32x4_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vstrwq_scatter_base_f32)))
void __arm_vstrwq_scatter_base_f32(uint32x4_t, int, float32x4_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vstrwq_scatter_base_f32)))
void __arm_vstrwq_scatter_base(uint32x4_t, int, float32x4_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vstrwq_scatter_base_p_f32)))
void __arm_vstrwq_scatter_base_p_f32(uint32x4_t, int, float32x4_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vstrwq_scatter_base_p_f32)))
void __arm_vstrwq_scatter_base_p(uint32x4_t, int, float32x4_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vstrwq_scatter_base_wb_f32)))
void __arm_vstrwq_scatter_base_wb_f32(uint32x4_t *, int, float32x4_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vstrwq_scatter_base_wb_f32)))
void __arm_vstrwq_scatter_base_wb(uint32x4_t *, int, float32x4_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vstrwq_scatter_base_wb_p_f32)))
void __arm_vstrwq_scatter_base_wb_p_f32(uint32x4_t *, int, float32x4_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vstrwq_scatter_base_wb_p_f32)))
void __arm_vstrwq_scatter_base_wb_p(uint32x4_t *, int, float32x4_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vstrwq_scatter_offset_f32)))
void __arm_vstrwq_scatter_offset_f32(float32_t *, uint32x4_t, float32x4_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vstrwq_scatter_offset_f32)))
void __arm_vstrwq_scatter_offset(float32_t *, uint32x4_t, float32x4_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vstrwq_scatter_offset_p_f32)))
void __arm_vstrwq_scatter_offset_p_f32(float32_t *, uint32x4_t, float32x4_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vstrwq_scatter_offset_p_f32)))
void __arm_vstrwq_scatter_offset_p(float32_t *, uint32x4_t, float32x4_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vstrwq_scatter_shifted_offset_f32)))
void __arm_vstrwq_scatter_shifted_offset_f32(float32_t *, uint32x4_t, float32x4_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vstrwq_scatter_shifted_offset_f32)))
void __arm_vstrwq_scatter_shifted_offset(float32_t *, uint32x4_t, float32x4_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vstrwq_scatter_shifted_offset_p_f32)))
void __arm_vstrwq_scatter_shifted_offset_p_f32(float32_t *, uint32x4_t, float32x4_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vstrwq_scatter_shifted_offset_p_f32)))
void __arm_vstrwq_scatter_shifted_offset_p(float32_t *, uint32x4_t, float32x4_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vsubq_f16)))
float16x8_t __arm_vsubq_f16(float16x8_t, float16x8_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vsubq_f16)))
float16x8_t __arm_vsubq(float16x8_t, float16x8_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vsubq_f32)))
float32x4_t __arm_vsubq_f32(float32x4_t, float32x4_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vsubq_f32)))
float32x4_t __arm_vsubq(float32x4_t, float32x4_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vsubq_m_f16)))
float16x8_t __arm_vsubq_m_f16(float16x8_t, float16x8_t, float16x8_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vsubq_m_f16)))
float16x8_t __arm_vsubq_m(float16x8_t, float16x8_t, float16x8_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vsubq_m_f32)))
float32x4_t __arm_vsubq_m_f32(float32x4_t, float32x4_t, float32x4_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vsubq_m_f32)))
float32x4_t __arm_vsubq_m(float32x4_t, float32x4_t, float32x4_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vuninitializedq_f16)))
float16x8_t __arm_vuninitializedq_f16();
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vuninitializedq_f32)))
float32x4_t __arm_vuninitializedq_f32();
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vuninitializedq_polymorphic_f16)))
float16x8_t __arm_vuninitializedq(float16x8_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vuninitializedq_polymorphic_f32)))
float32x4_t __arm_vuninitializedq(float32x4_t);

#endif /* (__ARM_FEATURE_MVE & 2) */

#if (!defined __ARM_MVE_PRESERVE_USER_NAMESPACE)

static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_asrl)))
int64_t asrl(int64_t, int32_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_lsll)))
uint64_t lsll(uint64_t, int32_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_sqrshr)))
int32_t sqrshr(int32_t, int32_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_sqrshrl)))
int64_t sqrshrl(int64_t, int32_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_sqrshrl_sat48)))
int64_t sqrshrl_sat48(int64_t, int32_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_sqshl)))
int32_t sqshl(int32_t, int);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_sqshll)))
int64_t sqshll(int64_t, int);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_srshr)))
int32_t srshr(int32_t, int);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_srshrl)))
int64_t srshrl(int64_t, int);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_uqrshl)))
uint32_t uqrshl(uint32_t, int32_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_uqrshll)))
uint64_t uqrshll(uint64_t, int32_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_uqrshll_sat48)))
uint64_t uqrshll_sat48(uint64_t, int32_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_uqshl)))
uint32_t uqshl(uint32_t, int);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_uqshll)))
uint64_t uqshll(uint64_t, int);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_urshr)))
uint32_t urshr(uint32_t, int);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_urshrl)))
uint64_t urshrl(uint64_t, int);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vabdq_m_s16)))
int16x8_t vabdq_m_s16(int16x8_t, int16x8_t, int16x8_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vabdq_m_s16)))
int16x8_t vabdq_m(int16x8_t, int16x8_t, int16x8_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vabdq_m_s32)))
int32x4_t vabdq_m_s32(int32x4_t, int32x4_t, int32x4_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vabdq_m_s32)))
int32x4_t vabdq_m(int32x4_t, int32x4_t, int32x4_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vabdq_m_s8)))
int8x16_t vabdq_m_s8(int8x16_t, int8x16_t, int8x16_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vabdq_m_s8)))
int8x16_t vabdq_m(int8x16_t, int8x16_t, int8x16_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vabdq_m_u16)))
uint16x8_t vabdq_m_u16(uint16x8_t, uint16x8_t, uint16x8_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vabdq_m_u16)))
uint16x8_t vabdq_m(uint16x8_t, uint16x8_t, uint16x8_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vabdq_m_u32)))
uint32x4_t vabdq_m_u32(uint32x4_t, uint32x4_t, uint32x4_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vabdq_m_u32)))
uint32x4_t vabdq_m(uint32x4_t, uint32x4_t, uint32x4_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vabdq_m_u8)))
uint8x16_t vabdq_m_u8(uint8x16_t, uint8x16_t, uint8x16_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vabdq_m_u8)))
uint8x16_t vabdq_m(uint8x16_t, uint8x16_t, uint8x16_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vabdq_s16)))
int16x8_t vabdq_s16(int16x8_t, int16x8_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vabdq_s16)))
int16x8_t vabdq(int16x8_t, int16x8_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vabdq_s32)))
int32x4_t vabdq_s32(int32x4_t, int32x4_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vabdq_s32)))
int32x4_t vabdq(int32x4_t, int32x4_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vabdq_s8)))
int8x16_t vabdq_s8(int8x16_t, int8x16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vabdq_s8)))
int8x16_t vabdq(int8x16_t, int8x16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vabdq_u16)))
uint16x8_t vabdq_u16(uint16x8_t, uint16x8_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vabdq_u16)))
uint16x8_t vabdq(uint16x8_t, uint16x8_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vabdq_u32)))
uint32x4_t vabdq_u32(uint32x4_t, uint32x4_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vabdq_u32)))
uint32x4_t vabdq(uint32x4_t, uint32x4_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vabdq_u8)))
uint8x16_t vabdq_u8(uint8x16_t, uint8x16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vabdq_u8)))
uint8x16_t vabdq(uint8x16_t, uint8x16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vadciq_m_s32)))
int32x4_t vadciq_m_s32(int32x4_t, int32x4_t, int32x4_t, unsigned *, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vadciq_m_s32)))
int32x4_t vadciq_m(int32x4_t, int32x4_t, int32x4_t, unsigned *, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vadciq_m_u32)))
uint32x4_t vadciq_m_u32(uint32x4_t, uint32x4_t, uint32x4_t, unsigned *, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vadciq_m_u32)))
uint32x4_t vadciq_m(uint32x4_t, uint32x4_t, uint32x4_t, unsigned *, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vadciq_s32)))
int32x4_t vadciq_s32(int32x4_t, int32x4_t, unsigned *);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vadciq_s32)))
int32x4_t vadciq(int32x4_t, int32x4_t, unsigned *);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vadciq_u32)))
uint32x4_t vadciq_u32(uint32x4_t, uint32x4_t, unsigned *);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vadciq_u32)))
uint32x4_t vadciq(uint32x4_t, uint32x4_t, unsigned *);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vadcq_m_s32)))
int32x4_t vadcq_m_s32(int32x4_t, int32x4_t, int32x4_t, unsigned *, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vadcq_m_s32)))
int32x4_t vadcq_m(int32x4_t, int32x4_t, int32x4_t, unsigned *, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vadcq_m_u32)))
uint32x4_t vadcq_m_u32(uint32x4_t, uint32x4_t, uint32x4_t, unsigned *, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vadcq_m_u32)))
uint32x4_t vadcq_m(uint32x4_t, uint32x4_t, uint32x4_t, unsigned *, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vadcq_s32)))
int32x4_t vadcq_s32(int32x4_t, int32x4_t, unsigned *);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vadcq_s32)))
int32x4_t vadcq(int32x4_t, int32x4_t, unsigned *);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vadcq_u32)))
uint32x4_t vadcq_u32(uint32x4_t, uint32x4_t, unsigned *);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vadcq_u32)))
uint32x4_t vadcq(uint32x4_t, uint32x4_t, unsigned *);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vaddq_m_s16)))
int16x8_t vaddq_m_s16(int16x8_t, int16x8_t, int16x8_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vaddq_m_s16)))
int16x8_t vaddq_m(int16x8_t, int16x8_t, int16x8_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vaddq_m_s32)))
int32x4_t vaddq_m_s32(int32x4_t, int32x4_t, int32x4_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vaddq_m_s32)))
int32x4_t vaddq_m(int32x4_t, int32x4_t, int32x4_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vaddq_m_s8)))
int8x16_t vaddq_m_s8(int8x16_t, int8x16_t, int8x16_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vaddq_m_s8)))
int8x16_t vaddq_m(int8x16_t, int8x16_t, int8x16_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vaddq_m_u16)))
uint16x8_t vaddq_m_u16(uint16x8_t, uint16x8_t, uint16x8_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vaddq_m_u16)))
uint16x8_t vaddq_m(uint16x8_t, uint16x8_t, uint16x8_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vaddq_m_u32)))
uint32x4_t vaddq_m_u32(uint32x4_t, uint32x4_t, uint32x4_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vaddq_m_u32)))
uint32x4_t vaddq_m(uint32x4_t, uint32x4_t, uint32x4_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vaddq_m_u8)))
uint8x16_t vaddq_m_u8(uint8x16_t, uint8x16_t, uint8x16_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vaddq_m_u8)))
uint8x16_t vaddq_m(uint8x16_t, uint8x16_t, uint8x16_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vaddq_s16)))
int16x8_t vaddq_s16(int16x8_t, int16x8_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vaddq_s16)))
int16x8_t vaddq(int16x8_t, int16x8_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vaddq_s32)))
int32x4_t vaddq_s32(int32x4_t, int32x4_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vaddq_s32)))
int32x4_t vaddq(int32x4_t, int32x4_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vaddq_s8)))
int8x16_t vaddq_s8(int8x16_t, int8x16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vaddq_s8)))
int8x16_t vaddq(int8x16_t, int8x16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vaddq_u16)))
uint16x8_t vaddq_u16(uint16x8_t, uint16x8_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vaddq_u16)))
uint16x8_t vaddq(uint16x8_t, uint16x8_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vaddq_u32)))
uint32x4_t vaddq_u32(uint32x4_t, uint32x4_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vaddq_u32)))
uint32x4_t vaddq(uint32x4_t, uint32x4_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vaddq_u8)))
uint8x16_t vaddq_u8(uint8x16_t, uint8x16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vaddq_u8)))
uint8x16_t vaddq(uint8x16_t, uint8x16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vandq_m_s16)))
int16x8_t vandq_m_s16(int16x8_t, int16x8_t, int16x8_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vandq_m_s16)))
int16x8_t vandq_m(int16x8_t, int16x8_t, int16x8_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vandq_m_s32)))
int32x4_t vandq_m_s32(int32x4_t, int32x4_t, int32x4_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vandq_m_s32)))
int32x4_t vandq_m(int32x4_t, int32x4_t, int32x4_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vandq_m_s8)))
int8x16_t vandq_m_s8(int8x16_t, int8x16_t, int8x16_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vandq_m_s8)))
int8x16_t vandq_m(int8x16_t, int8x16_t, int8x16_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vandq_m_u16)))
uint16x8_t vandq_m_u16(uint16x8_t, uint16x8_t, uint16x8_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vandq_m_u16)))
uint16x8_t vandq_m(uint16x8_t, uint16x8_t, uint16x8_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vandq_m_u32)))
uint32x4_t vandq_m_u32(uint32x4_t, uint32x4_t, uint32x4_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vandq_m_u32)))
uint32x4_t vandq_m(uint32x4_t, uint32x4_t, uint32x4_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vandq_m_u8)))
uint8x16_t vandq_m_u8(uint8x16_t, uint8x16_t, uint8x16_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vandq_m_u8)))
uint8x16_t vandq_m(uint8x16_t, uint8x16_t, uint8x16_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vandq_s16)))
int16x8_t vandq_s16(int16x8_t, int16x8_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vandq_s16)))
int16x8_t vandq(int16x8_t, int16x8_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vandq_s32)))
int32x4_t vandq_s32(int32x4_t, int32x4_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vandq_s32)))
int32x4_t vandq(int32x4_t, int32x4_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vandq_s8)))
int8x16_t vandq_s8(int8x16_t, int8x16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vandq_s8)))
int8x16_t vandq(int8x16_t, int8x16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vandq_u16)))
uint16x8_t vandq_u16(uint16x8_t, uint16x8_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vandq_u16)))
uint16x8_t vandq(uint16x8_t, uint16x8_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vandq_u32)))
uint32x4_t vandq_u32(uint32x4_t, uint32x4_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vandq_u32)))
uint32x4_t vandq(uint32x4_t, uint32x4_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vandq_u8)))
uint8x16_t vandq_u8(uint8x16_t, uint8x16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vandq_u8)))
uint8x16_t vandq(uint8x16_t, uint8x16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vbicq_m_s16)))
int16x8_t vbicq_m_s16(int16x8_t, int16x8_t, int16x8_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vbicq_m_s16)))
int16x8_t vbicq_m(int16x8_t, int16x8_t, int16x8_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vbicq_m_s32)))
int32x4_t vbicq_m_s32(int32x4_t, int32x4_t, int32x4_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vbicq_m_s32)))
int32x4_t vbicq_m(int32x4_t, int32x4_t, int32x4_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vbicq_m_s8)))
int8x16_t vbicq_m_s8(int8x16_t, int8x16_t, int8x16_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vbicq_m_s8)))
int8x16_t vbicq_m(int8x16_t, int8x16_t, int8x16_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vbicq_m_u16)))
uint16x8_t vbicq_m_u16(uint16x8_t, uint16x8_t, uint16x8_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vbicq_m_u16)))
uint16x8_t vbicq_m(uint16x8_t, uint16x8_t, uint16x8_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vbicq_m_u32)))
uint32x4_t vbicq_m_u32(uint32x4_t, uint32x4_t, uint32x4_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vbicq_m_u32)))
uint32x4_t vbicq_m(uint32x4_t, uint32x4_t, uint32x4_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vbicq_m_u8)))
uint8x16_t vbicq_m_u8(uint8x16_t, uint8x16_t, uint8x16_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vbicq_m_u8)))
uint8x16_t vbicq_m(uint8x16_t, uint8x16_t, uint8x16_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vbicq_s16)))
int16x8_t vbicq_s16(int16x8_t, int16x8_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vbicq_s16)))
int16x8_t vbicq(int16x8_t, int16x8_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vbicq_s32)))
int32x4_t vbicq_s32(int32x4_t, int32x4_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vbicq_s32)))
int32x4_t vbicq(int32x4_t, int32x4_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vbicq_s8)))
int8x16_t vbicq_s8(int8x16_t, int8x16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vbicq_s8)))
int8x16_t vbicq(int8x16_t, int8x16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vbicq_u16)))
uint16x8_t vbicq_u16(uint16x8_t, uint16x8_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vbicq_u16)))
uint16x8_t vbicq(uint16x8_t, uint16x8_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vbicq_u32)))
uint32x4_t vbicq_u32(uint32x4_t, uint32x4_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vbicq_u32)))
uint32x4_t vbicq(uint32x4_t, uint32x4_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vbicq_u8)))
uint8x16_t vbicq_u8(uint8x16_t, uint8x16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vbicq_u8)))
uint8x16_t vbicq(uint8x16_t, uint8x16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpcsq_m_n_u16)))
mve_pred16_t vcmpcsq_m_n_u16(uint16x8_t, uint16_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpcsq_m_n_u16)))
mve_pred16_t vcmpcsq_m(uint16x8_t, uint16_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpcsq_m_n_u32)))
mve_pred16_t vcmpcsq_m_n_u32(uint32x4_t, uint32_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpcsq_m_n_u32)))
mve_pred16_t vcmpcsq_m(uint32x4_t, uint32_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpcsq_m_n_u8)))
mve_pred16_t vcmpcsq_m_n_u8(uint8x16_t, uint8_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpcsq_m_n_u8)))
mve_pred16_t vcmpcsq_m(uint8x16_t, uint8_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpcsq_m_u16)))
mve_pred16_t vcmpcsq_m_u16(uint16x8_t, uint16x8_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpcsq_m_u16)))
mve_pred16_t vcmpcsq_m(uint16x8_t, uint16x8_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpcsq_m_u32)))
mve_pred16_t vcmpcsq_m_u32(uint32x4_t, uint32x4_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpcsq_m_u32)))
mve_pred16_t vcmpcsq_m(uint32x4_t, uint32x4_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpcsq_m_u8)))
mve_pred16_t vcmpcsq_m_u8(uint8x16_t, uint8x16_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpcsq_m_u8)))
mve_pred16_t vcmpcsq_m(uint8x16_t, uint8x16_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpcsq_n_u16)))
mve_pred16_t vcmpcsq_n_u16(uint16x8_t, uint16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpcsq_n_u16)))
mve_pred16_t vcmpcsq(uint16x8_t, uint16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpcsq_n_u32)))
mve_pred16_t vcmpcsq_n_u32(uint32x4_t, uint32_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpcsq_n_u32)))
mve_pred16_t vcmpcsq(uint32x4_t, uint32_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpcsq_n_u8)))
mve_pred16_t vcmpcsq_n_u8(uint8x16_t, uint8_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpcsq_n_u8)))
mve_pred16_t vcmpcsq(uint8x16_t, uint8_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpcsq_u16)))
mve_pred16_t vcmpcsq_u16(uint16x8_t, uint16x8_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpcsq_u16)))
mve_pred16_t vcmpcsq(uint16x8_t, uint16x8_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpcsq_u32)))
mve_pred16_t vcmpcsq_u32(uint32x4_t, uint32x4_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpcsq_u32)))
mve_pred16_t vcmpcsq(uint32x4_t, uint32x4_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpcsq_u8)))
mve_pred16_t vcmpcsq_u8(uint8x16_t, uint8x16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpcsq_u8)))
mve_pred16_t vcmpcsq(uint8x16_t, uint8x16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpeqq_m_n_s16)))
mve_pred16_t vcmpeqq_m_n_s16(int16x8_t, int16_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpeqq_m_n_s16)))
mve_pred16_t vcmpeqq_m(int16x8_t, int16_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpeqq_m_n_s32)))
mve_pred16_t vcmpeqq_m_n_s32(int32x4_t, int32_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpeqq_m_n_s32)))
mve_pred16_t vcmpeqq_m(int32x4_t, int32_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpeqq_m_n_s8)))
mve_pred16_t vcmpeqq_m_n_s8(int8x16_t, int8_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpeqq_m_n_s8)))
mve_pred16_t vcmpeqq_m(int8x16_t, int8_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpeqq_m_n_u16)))
mve_pred16_t vcmpeqq_m_n_u16(uint16x8_t, uint16_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpeqq_m_n_u16)))
mve_pred16_t vcmpeqq_m(uint16x8_t, uint16_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpeqq_m_n_u32)))
mve_pred16_t vcmpeqq_m_n_u32(uint32x4_t, uint32_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpeqq_m_n_u32)))
mve_pred16_t vcmpeqq_m(uint32x4_t, uint32_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpeqq_m_n_u8)))
mve_pred16_t vcmpeqq_m_n_u8(uint8x16_t, uint8_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpeqq_m_n_u8)))
mve_pred16_t vcmpeqq_m(uint8x16_t, uint8_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpeqq_m_s16)))
mve_pred16_t vcmpeqq_m_s16(int16x8_t, int16x8_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpeqq_m_s16)))
mve_pred16_t vcmpeqq_m(int16x8_t, int16x8_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpeqq_m_s32)))
mve_pred16_t vcmpeqq_m_s32(int32x4_t, int32x4_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpeqq_m_s32)))
mve_pred16_t vcmpeqq_m(int32x4_t, int32x4_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpeqq_m_s8)))
mve_pred16_t vcmpeqq_m_s8(int8x16_t, int8x16_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpeqq_m_s8)))
mve_pred16_t vcmpeqq_m(int8x16_t, int8x16_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpeqq_m_u16)))
mve_pred16_t vcmpeqq_m_u16(uint16x8_t, uint16x8_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpeqq_m_u16)))
mve_pred16_t vcmpeqq_m(uint16x8_t, uint16x8_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpeqq_m_u32)))
mve_pred16_t vcmpeqq_m_u32(uint32x4_t, uint32x4_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpeqq_m_u32)))
mve_pred16_t vcmpeqq_m(uint32x4_t, uint32x4_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpeqq_m_u8)))
mve_pred16_t vcmpeqq_m_u8(uint8x16_t, uint8x16_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpeqq_m_u8)))
mve_pred16_t vcmpeqq_m(uint8x16_t, uint8x16_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpeqq_n_s16)))
mve_pred16_t vcmpeqq_n_s16(int16x8_t, int16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpeqq_n_s16)))
mve_pred16_t vcmpeqq(int16x8_t, int16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpeqq_n_s32)))
mve_pred16_t vcmpeqq_n_s32(int32x4_t, int32_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpeqq_n_s32)))
mve_pred16_t vcmpeqq(int32x4_t, int32_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpeqq_n_s8)))
mve_pred16_t vcmpeqq_n_s8(int8x16_t, int8_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpeqq_n_s8)))
mve_pred16_t vcmpeqq(int8x16_t, int8_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpeqq_n_u16)))
mve_pred16_t vcmpeqq_n_u16(uint16x8_t, uint16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpeqq_n_u16)))
mve_pred16_t vcmpeqq(uint16x8_t, uint16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpeqq_n_u32)))
mve_pred16_t vcmpeqq_n_u32(uint32x4_t, uint32_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpeqq_n_u32)))
mve_pred16_t vcmpeqq(uint32x4_t, uint32_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpeqq_n_u8)))
mve_pred16_t vcmpeqq_n_u8(uint8x16_t, uint8_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpeqq_n_u8)))
mve_pred16_t vcmpeqq(uint8x16_t, uint8_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpeqq_s16)))
mve_pred16_t vcmpeqq_s16(int16x8_t, int16x8_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpeqq_s16)))
mve_pred16_t vcmpeqq(int16x8_t, int16x8_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpeqq_s32)))
mve_pred16_t vcmpeqq_s32(int32x4_t, int32x4_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpeqq_s32)))
mve_pred16_t vcmpeqq(int32x4_t, int32x4_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpeqq_s8)))
mve_pred16_t vcmpeqq_s8(int8x16_t, int8x16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpeqq_s8)))
mve_pred16_t vcmpeqq(int8x16_t, int8x16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpeqq_u16)))
mve_pred16_t vcmpeqq_u16(uint16x8_t, uint16x8_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpeqq_u16)))
mve_pred16_t vcmpeqq(uint16x8_t, uint16x8_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpeqq_u32)))
mve_pred16_t vcmpeqq_u32(uint32x4_t, uint32x4_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpeqq_u32)))
mve_pred16_t vcmpeqq(uint32x4_t, uint32x4_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpeqq_u8)))
mve_pred16_t vcmpeqq_u8(uint8x16_t, uint8x16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpeqq_u8)))
mve_pred16_t vcmpeqq(uint8x16_t, uint8x16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpgeq_m_n_s16)))
mve_pred16_t vcmpgeq_m_n_s16(int16x8_t, int16_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpgeq_m_n_s16)))
mve_pred16_t vcmpgeq_m(int16x8_t, int16_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpgeq_m_n_s32)))
mve_pred16_t vcmpgeq_m_n_s32(int32x4_t, int32_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpgeq_m_n_s32)))
mve_pred16_t vcmpgeq_m(int32x4_t, int32_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpgeq_m_n_s8)))
mve_pred16_t vcmpgeq_m_n_s8(int8x16_t, int8_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpgeq_m_n_s8)))
mve_pred16_t vcmpgeq_m(int8x16_t, int8_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpgeq_m_s16)))
mve_pred16_t vcmpgeq_m_s16(int16x8_t, int16x8_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpgeq_m_s16)))
mve_pred16_t vcmpgeq_m(int16x8_t, int16x8_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpgeq_m_s32)))
mve_pred16_t vcmpgeq_m_s32(int32x4_t, int32x4_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpgeq_m_s32)))
mve_pred16_t vcmpgeq_m(int32x4_t, int32x4_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpgeq_m_s8)))
mve_pred16_t vcmpgeq_m_s8(int8x16_t, int8x16_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpgeq_m_s8)))
mve_pred16_t vcmpgeq_m(int8x16_t, int8x16_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpgeq_n_s16)))
mve_pred16_t vcmpgeq_n_s16(int16x8_t, int16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpgeq_n_s16)))
mve_pred16_t vcmpgeq(int16x8_t, int16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpgeq_n_s32)))
mve_pred16_t vcmpgeq_n_s32(int32x4_t, int32_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpgeq_n_s32)))
mve_pred16_t vcmpgeq(int32x4_t, int32_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpgeq_n_s8)))
mve_pred16_t vcmpgeq_n_s8(int8x16_t, int8_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpgeq_n_s8)))
mve_pred16_t vcmpgeq(int8x16_t, int8_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpgeq_s16)))
mve_pred16_t vcmpgeq_s16(int16x8_t, int16x8_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpgeq_s16)))
mve_pred16_t vcmpgeq(int16x8_t, int16x8_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpgeq_s32)))
mve_pred16_t vcmpgeq_s32(int32x4_t, int32x4_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpgeq_s32)))
mve_pred16_t vcmpgeq(int32x4_t, int32x4_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpgeq_s8)))
mve_pred16_t vcmpgeq_s8(int8x16_t, int8x16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpgeq_s8)))
mve_pred16_t vcmpgeq(int8x16_t, int8x16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpgtq_m_n_s16)))
mve_pred16_t vcmpgtq_m_n_s16(int16x8_t, int16_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpgtq_m_n_s16)))
mve_pred16_t vcmpgtq_m(int16x8_t, int16_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpgtq_m_n_s32)))
mve_pred16_t vcmpgtq_m_n_s32(int32x4_t, int32_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpgtq_m_n_s32)))
mve_pred16_t vcmpgtq_m(int32x4_t, int32_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpgtq_m_n_s8)))
mve_pred16_t vcmpgtq_m_n_s8(int8x16_t, int8_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpgtq_m_n_s8)))
mve_pred16_t vcmpgtq_m(int8x16_t, int8_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpgtq_m_s16)))
mve_pred16_t vcmpgtq_m_s16(int16x8_t, int16x8_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpgtq_m_s16)))
mve_pred16_t vcmpgtq_m(int16x8_t, int16x8_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpgtq_m_s32)))
mve_pred16_t vcmpgtq_m_s32(int32x4_t, int32x4_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpgtq_m_s32)))
mve_pred16_t vcmpgtq_m(int32x4_t, int32x4_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpgtq_m_s8)))
mve_pred16_t vcmpgtq_m_s8(int8x16_t, int8x16_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpgtq_m_s8)))
mve_pred16_t vcmpgtq_m(int8x16_t, int8x16_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpgtq_n_s16)))
mve_pred16_t vcmpgtq_n_s16(int16x8_t, int16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpgtq_n_s16)))
mve_pred16_t vcmpgtq(int16x8_t, int16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpgtq_n_s32)))
mve_pred16_t vcmpgtq_n_s32(int32x4_t, int32_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpgtq_n_s32)))
mve_pred16_t vcmpgtq(int32x4_t, int32_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpgtq_n_s8)))
mve_pred16_t vcmpgtq_n_s8(int8x16_t, int8_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpgtq_n_s8)))
mve_pred16_t vcmpgtq(int8x16_t, int8_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpgtq_s16)))
mve_pred16_t vcmpgtq_s16(int16x8_t, int16x8_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpgtq_s16)))
mve_pred16_t vcmpgtq(int16x8_t, int16x8_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpgtq_s32)))
mve_pred16_t vcmpgtq_s32(int32x4_t, int32x4_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpgtq_s32)))
mve_pred16_t vcmpgtq(int32x4_t, int32x4_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpgtq_s8)))
mve_pred16_t vcmpgtq_s8(int8x16_t, int8x16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpgtq_s8)))
mve_pred16_t vcmpgtq(int8x16_t, int8x16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmphiq_m_n_u16)))
mve_pred16_t vcmphiq_m_n_u16(uint16x8_t, uint16_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmphiq_m_n_u16)))
mve_pred16_t vcmphiq_m(uint16x8_t, uint16_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmphiq_m_n_u32)))
mve_pred16_t vcmphiq_m_n_u32(uint32x4_t, uint32_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmphiq_m_n_u32)))
mve_pred16_t vcmphiq_m(uint32x4_t, uint32_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmphiq_m_n_u8)))
mve_pred16_t vcmphiq_m_n_u8(uint8x16_t, uint8_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmphiq_m_n_u8)))
mve_pred16_t vcmphiq_m(uint8x16_t, uint8_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmphiq_m_u16)))
mve_pred16_t vcmphiq_m_u16(uint16x8_t, uint16x8_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmphiq_m_u16)))
mve_pred16_t vcmphiq_m(uint16x8_t, uint16x8_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmphiq_m_u32)))
mve_pred16_t vcmphiq_m_u32(uint32x4_t, uint32x4_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmphiq_m_u32)))
mve_pred16_t vcmphiq_m(uint32x4_t, uint32x4_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmphiq_m_u8)))
mve_pred16_t vcmphiq_m_u8(uint8x16_t, uint8x16_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmphiq_m_u8)))
mve_pred16_t vcmphiq_m(uint8x16_t, uint8x16_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmphiq_n_u16)))
mve_pred16_t vcmphiq_n_u16(uint16x8_t, uint16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmphiq_n_u16)))
mve_pred16_t vcmphiq(uint16x8_t, uint16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmphiq_n_u32)))
mve_pred16_t vcmphiq_n_u32(uint32x4_t, uint32_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmphiq_n_u32)))
mve_pred16_t vcmphiq(uint32x4_t, uint32_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmphiq_n_u8)))
mve_pred16_t vcmphiq_n_u8(uint8x16_t, uint8_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmphiq_n_u8)))
mve_pred16_t vcmphiq(uint8x16_t, uint8_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmphiq_u16)))
mve_pred16_t vcmphiq_u16(uint16x8_t, uint16x8_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmphiq_u16)))
mve_pred16_t vcmphiq(uint16x8_t, uint16x8_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmphiq_u32)))
mve_pred16_t vcmphiq_u32(uint32x4_t, uint32x4_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmphiq_u32)))
mve_pred16_t vcmphiq(uint32x4_t, uint32x4_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmphiq_u8)))
mve_pred16_t vcmphiq_u8(uint8x16_t, uint8x16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmphiq_u8)))
mve_pred16_t vcmphiq(uint8x16_t, uint8x16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpleq_m_n_s16)))
mve_pred16_t vcmpleq_m_n_s16(int16x8_t, int16_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpleq_m_n_s16)))
mve_pred16_t vcmpleq_m(int16x8_t, int16_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpleq_m_n_s32)))
mve_pred16_t vcmpleq_m_n_s32(int32x4_t, int32_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpleq_m_n_s32)))
mve_pred16_t vcmpleq_m(int32x4_t, int32_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpleq_m_n_s8)))
mve_pred16_t vcmpleq_m_n_s8(int8x16_t, int8_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpleq_m_n_s8)))
mve_pred16_t vcmpleq_m(int8x16_t, int8_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpleq_m_s16)))
mve_pred16_t vcmpleq_m_s16(int16x8_t, int16x8_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpleq_m_s16)))
mve_pred16_t vcmpleq_m(int16x8_t, int16x8_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpleq_m_s32)))
mve_pred16_t vcmpleq_m_s32(int32x4_t, int32x4_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpleq_m_s32)))
mve_pred16_t vcmpleq_m(int32x4_t, int32x4_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpleq_m_s8)))
mve_pred16_t vcmpleq_m_s8(int8x16_t, int8x16_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpleq_m_s8)))
mve_pred16_t vcmpleq_m(int8x16_t, int8x16_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpleq_n_s16)))
mve_pred16_t vcmpleq_n_s16(int16x8_t, int16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpleq_n_s16)))
mve_pred16_t vcmpleq(int16x8_t, int16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpleq_n_s32)))
mve_pred16_t vcmpleq_n_s32(int32x4_t, int32_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpleq_n_s32)))
mve_pred16_t vcmpleq(int32x4_t, int32_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpleq_n_s8)))
mve_pred16_t vcmpleq_n_s8(int8x16_t, int8_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpleq_n_s8)))
mve_pred16_t vcmpleq(int8x16_t, int8_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpleq_s16)))
mve_pred16_t vcmpleq_s16(int16x8_t, int16x8_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpleq_s16)))
mve_pred16_t vcmpleq(int16x8_t, int16x8_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpleq_s32)))
mve_pred16_t vcmpleq_s32(int32x4_t, int32x4_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpleq_s32)))
mve_pred16_t vcmpleq(int32x4_t, int32x4_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpleq_s8)))
mve_pred16_t vcmpleq_s8(int8x16_t, int8x16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpleq_s8)))
mve_pred16_t vcmpleq(int8x16_t, int8x16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpltq_m_n_s16)))
mve_pred16_t vcmpltq_m_n_s16(int16x8_t, int16_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpltq_m_n_s16)))
mve_pred16_t vcmpltq_m(int16x8_t, int16_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpltq_m_n_s32)))
mve_pred16_t vcmpltq_m_n_s32(int32x4_t, int32_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpltq_m_n_s32)))
mve_pred16_t vcmpltq_m(int32x4_t, int32_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpltq_m_n_s8)))
mve_pred16_t vcmpltq_m_n_s8(int8x16_t, int8_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpltq_m_n_s8)))
mve_pred16_t vcmpltq_m(int8x16_t, int8_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpltq_m_s16)))
mve_pred16_t vcmpltq_m_s16(int16x8_t, int16x8_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpltq_m_s16)))
mve_pred16_t vcmpltq_m(int16x8_t, int16x8_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpltq_m_s32)))
mve_pred16_t vcmpltq_m_s32(int32x4_t, int32x4_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpltq_m_s32)))
mve_pred16_t vcmpltq_m(int32x4_t, int32x4_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpltq_m_s8)))
mve_pred16_t vcmpltq_m_s8(int8x16_t, int8x16_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpltq_m_s8)))
mve_pred16_t vcmpltq_m(int8x16_t, int8x16_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpltq_n_s16)))
mve_pred16_t vcmpltq_n_s16(int16x8_t, int16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpltq_n_s16)))
mve_pred16_t vcmpltq(int16x8_t, int16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpltq_n_s32)))
mve_pred16_t vcmpltq_n_s32(int32x4_t, int32_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpltq_n_s32)))
mve_pred16_t vcmpltq(int32x4_t, int32_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpltq_n_s8)))
mve_pred16_t vcmpltq_n_s8(int8x16_t, int8_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpltq_n_s8)))
mve_pred16_t vcmpltq(int8x16_t, int8_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpltq_s16)))
mve_pred16_t vcmpltq_s16(int16x8_t, int16x8_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpltq_s16)))
mve_pred16_t vcmpltq(int16x8_t, int16x8_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpltq_s32)))
mve_pred16_t vcmpltq_s32(int32x4_t, int32x4_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpltq_s32)))
mve_pred16_t vcmpltq(int32x4_t, int32x4_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpltq_s8)))
mve_pred16_t vcmpltq_s8(int8x16_t, int8x16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpltq_s8)))
mve_pred16_t vcmpltq(int8x16_t, int8x16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpneq_m_n_s16)))
mve_pred16_t vcmpneq_m_n_s16(int16x8_t, int16_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpneq_m_n_s16)))
mve_pred16_t vcmpneq_m(int16x8_t, int16_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpneq_m_n_s32)))
mve_pred16_t vcmpneq_m_n_s32(int32x4_t, int32_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpneq_m_n_s32)))
mve_pred16_t vcmpneq_m(int32x4_t, int32_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpneq_m_n_s8)))
mve_pred16_t vcmpneq_m_n_s8(int8x16_t, int8_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpneq_m_n_s8)))
mve_pred16_t vcmpneq_m(int8x16_t, int8_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpneq_m_n_u16)))
mve_pred16_t vcmpneq_m_n_u16(uint16x8_t, uint16_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpneq_m_n_u16)))
mve_pred16_t vcmpneq_m(uint16x8_t, uint16_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpneq_m_n_u32)))
mve_pred16_t vcmpneq_m_n_u32(uint32x4_t, uint32_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpneq_m_n_u32)))
mve_pred16_t vcmpneq_m(uint32x4_t, uint32_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpneq_m_n_u8)))
mve_pred16_t vcmpneq_m_n_u8(uint8x16_t, uint8_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpneq_m_n_u8)))
mve_pred16_t vcmpneq_m(uint8x16_t, uint8_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpneq_m_s16)))
mve_pred16_t vcmpneq_m_s16(int16x8_t, int16x8_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpneq_m_s16)))
mve_pred16_t vcmpneq_m(int16x8_t, int16x8_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpneq_m_s32)))
mve_pred16_t vcmpneq_m_s32(int32x4_t, int32x4_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpneq_m_s32)))
mve_pred16_t vcmpneq_m(int32x4_t, int32x4_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpneq_m_s8)))
mve_pred16_t vcmpneq_m_s8(int8x16_t, int8x16_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpneq_m_s8)))
mve_pred16_t vcmpneq_m(int8x16_t, int8x16_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpneq_m_u16)))
mve_pred16_t vcmpneq_m_u16(uint16x8_t, uint16x8_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpneq_m_u16)))
mve_pred16_t vcmpneq_m(uint16x8_t, uint16x8_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpneq_m_u32)))
mve_pred16_t vcmpneq_m_u32(uint32x4_t, uint32x4_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpneq_m_u32)))
mve_pred16_t vcmpneq_m(uint32x4_t, uint32x4_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpneq_m_u8)))
mve_pred16_t vcmpneq_m_u8(uint8x16_t, uint8x16_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpneq_m_u8)))
mve_pred16_t vcmpneq_m(uint8x16_t, uint8x16_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpneq_n_s16)))
mve_pred16_t vcmpneq_n_s16(int16x8_t, int16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpneq_n_s16)))
mve_pred16_t vcmpneq(int16x8_t, int16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpneq_n_s32)))
mve_pred16_t vcmpneq_n_s32(int32x4_t, int32_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpneq_n_s32)))
mve_pred16_t vcmpneq(int32x4_t, int32_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpneq_n_s8)))
mve_pred16_t vcmpneq_n_s8(int8x16_t, int8_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpneq_n_s8)))
mve_pred16_t vcmpneq(int8x16_t, int8_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpneq_n_u16)))
mve_pred16_t vcmpneq_n_u16(uint16x8_t, uint16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpneq_n_u16)))
mve_pred16_t vcmpneq(uint16x8_t, uint16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpneq_n_u32)))
mve_pred16_t vcmpneq_n_u32(uint32x4_t, uint32_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpneq_n_u32)))
mve_pred16_t vcmpneq(uint32x4_t, uint32_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpneq_n_u8)))
mve_pred16_t vcmpneq_n_u8(uint8x16_t, uint8_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpneq_n_u8)))
mve_pred16_t vcmpneq(uint8x16_t, uint8_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpneq_s16)))
mve_pred16_t vcmpneq_s16(int16x8_t, int16x8_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpneq_s16)))
mve_pred16_t vcmpneq(int16x8_t, int16x8_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpneq_s32)))
mve_pred16_t vcmpneq_s32(int32x4_t, int32x4_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpneq_s32)))
mve_pred16_t vcmpneq(int32x4_t, int32x4_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpneq_s8)))
mve_pred16_t vcmpneq_s8(int8x16_t, int8x16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpneq_s8)))
mve_pred16_t vcmpneq(int8x16_t, int8x16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpneq_u16)))
mve_pred16_t vcmpneq_u16(uint16x8_t, uint16x8_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpneq_u16)))
mve_pred16_t vcmpneq(uint16x8_t, uint16x8_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpneq_u32)))
mve_pred16_t vcmpneq_u32(uint32x4_t, uint32x4_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpneq_u32)))
mve_pred16_t vcmpneq(uint32x4_t, uint32x4_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpneq_u8)))
mve_pred16_t vcmpneq_u8(uint8x16_t, uint8x16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpneq_u8)))
mve_pred16_t vcmpneq(uint8x16_t, uint8x16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcreateq_s16)))
int16x8_t vcreateq_s16(uint64_t, uint64_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcreateq_s32)))
int32x4_t vcreateq_s32(uint64_t, uint64_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcreateq_s64)))
int64x2_t vcreateq_s64(uint64_t, uint64_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcreateq_s8)))
int8x16_t vcreateq_s8(uint64_t, uint64_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcreateq_u16)))
uint16x8_t vcreateq_u16(uint64_t, uint64_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcreateq_u32)))
uint32x4_t vcreateq_u32(uint64_t, uint64_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcreateq_u64)))
uint64x2_t vcreateq_u64(uint64_t, uint64_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcreateq_u8)))
uint8x16_t vcreateq_u8(uint64_t, uint64_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_veorq_m_s16)))
int16x8_t veorq_m_s16(int16x8_t, int16x8_t, int16x8_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_veorq_m_s16)))
int16x8_t veorq_m(int16x8_t, int16x8_t, int16x8_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_veorq_m_s32)))
int32x4_t veorq_m_s32(int32x4_t, int32x4_t, int32x4_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_veorq_m_s32)))
int32x4_t veorq_m(int32x4_t, int32x4_t, int32x4_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_veorq_m_s8)))
int8x16_t veorq_m_s8(int8x16_t, int8x16_t, int8x16_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_veorq_m_s8)))
int8x16_t veorq_m(int8x16_t, int8x16_t, int8x16_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_veorq_m_u16)))
uint16x8_t veorq_m_u16(uint16x8_t, uint16x8_t, uint16x8_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_veorq_m_u16)))
uint16x8_t veorq_m(uint16x8_t, uint16x8_t, uint16x8_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_veorq_m_u32)))
uint32x4_t veorq_m_u32(uint32x4_t, uint32x4_t, uint32x4_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_veorq_m_u32)))
uint32x4_t veorq_m(uint32x4_t, uint32x4_t, uint32x4_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_veorq_m_u8)))
uint8x16_t veorq_m_u8(uint8x16_t, uint8x16_t, uint8x16_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_veorq_m_u8)))
uint8x16_t veorq_m(uint8x16_t, uint8x16_t, uint8x16_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_veorq_s16)))
int16x8_t veorq_s16(int16x8_t, int16x8_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_veorq_s16)))
int16x8_t veorq(int16x8_t, int16x8_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_veorq_s32)))
int32x4_t veorq_s32(int32x4_t, int32x4_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_veorq_s32)))
int32x4_t veorq(int32x4_t, int32x4_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_veorq_s8)))
int8x16_t veorq_s8(int8x16_t, int8x16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_veorq_s8)))
int8x16_t veorq(int8x16_t, int8x16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_veorq_u16)))
uint16x8_t veorq_u16(uint16x8_t, uint16x8_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_veorq_u16)))
uint16x8_t veorq(uint16x8_t, uint16x8_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_veorq_u32)))
uint32x4_t veorq_u32(uint32x4_t, uint32x4_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_veorq_u32)))
uint32x4_t veorq(uint32x4_t, uint32x4_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_veorq_u8)))
uint8x16_t veorq_u8(uint8x16_t, uint8x16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_veorq_u8)))
uint8x16_t veorq(uint8x16_t, uint8x16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vgetq_lane_s16)))
int16_t vgetq_lane_s16(int16x8_t, int);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vgetq_lane_s16)))
int16_t vgetq_lane(int16x8_t, int);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vgetq_lane_s32)))
int32_t vgetq_lane_s32(int32x4_t, int);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vgetq_lane_s32)))
int32_t vgetq_lane(int32x4_t, int);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vgetq_lane_s64)))
int64_t vgetq_lane_s64(int64x2_t, int);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vgetq_lane_s64)))
int64_t vgetq_lane(int64x2_t, int);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vgetq_lane_s8)))
int8_t vgetq_lane_s8(int8x16_t, int);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vgetq_lane_s8)))
int8_t vgetq_lane(int8x16_t, int);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vgetq_lane_u16)))
uint16_t vgetq_lane_u16(uint16x8_t, int);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vgetq_lane_u16)))
uint16_t vgetq_lane(uint16x8_t, int);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vgetq_lane_u32)))
uint32_t vgetq_lane_u32(uint32x4_t, int);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vgetq_lane_u32)))
uint32_t vgetq_lane(uint32x4_t, int);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vgetq_lane_u64)))
uint64_t vgetq_lane_u64(uint64x2_t, int);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vgetq_lane_u64)))
uint64_t vgetq_lane(uint64x2_t, int);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vgetq_lane_u8)))
uint8_t vgetq_lane_u8(uint8x16_t, int);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vgetq_lane_u8)))
uint8_t vgetq_lane(uint8x16_t, int);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vld1q_s16)))
int16x8_t vld1q_s16(const int16_t *);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vld1q_s16)))
int16x8_t vld1q(const int16_t *);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vld1q_s32)))
int32x4_t vld1q_s32(const int32_t *);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vld1q_s32)))
int32x4_t vld1q(const int32_t *);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vld1q_s8)))
int8x16_t vld1q_s8(const int8_t *);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vld1q_s8)))
int8x16_t vld1q(const int8_t *);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vld1q_u16)))
uint16x8_t vld1q_u16(const uint16_t *);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vld1q_u16)))
uint16x8_t vld1q(const uint16_t *);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vld1q_u32)))
uint32x4_t vld1q_u32(const uint32_t *);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vld1q_u32)))
uint32x4_t vld1q(const uint32_t *);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vld1q_u8)))
uint8x16_t vld1q_u8(const uint8_t *);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vld1q_u8)))
uint8x16_t vld1q(const uint8_t *);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vld1q_z_s16)))
int16x8_t vld1q_z_s16(const int16_t *, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vld1q_z_s16)))
int16x8_t vld1q_z(const int16_t *, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vld1q_z_s32)))
int32x4_t vld1q_z_s32(const int32_t *, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vld1q_z_s32)))
int32x4_t vld1q_z(const int32_t *, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vld1q_z_s8)))
int8x16_t vld1q_z_s8(const int8_t *, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vld1q_z_s8)))
int8x16_t vld1q_z(const int8_t *, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vld1q_z_u16)))
uint16x8_t vld1q_z_u16(const uint16_t *, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vld1q_z_u16)))
uint16x8_t vld1q_z(const uint16_t *, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vld1q_z_u32)))
uint32x4_t vld1q_z_u32(const uint32_t *, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vld1q_z_u32)))
uint32x4_t vld1q_z(const uint32_t *, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vld1q_z_u8)))
uint8x16_t vld1q_z_u8(const uint8_t *, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vld1q_z_u8)))
uint8x16_t vld1q_z(const uint8_t *, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vld2q_s16)))
int16x8x2_t vld2q_s16(const int16_t *);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vld2q_s16)))
int16x8x2_t vld2q(const int16_t *);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vld2q_s32)))
int32x4x2_t vld2q_s32(const int32_t *);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vld2q_s32)))
int32x4x2_t vld2q(const int32_t *);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vld2q_s8)))
int8x16x2_t vld2q_s8(const int8_t *);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vld2q_s8)))
int8x16x2_t vld2q(const int8_t *);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vld2q_u16)))
uint16x8x2_t vld2q_u16(const uint16_t *);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vld2q_u16)))
uint16x8x2_t vld2q(const uint16_t *);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vld2q_u32)))
uint32x4x2_t vld2q_u32(const uint32_t *);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vld2q_u32)))
uint32x4x2_t vld2q(const uint32_t *);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vld2q_u8)))
uint8x16x2_t vld2q_u8(const uint8_t *);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vld2q_u8)))
uint8x16x2_t vld2q(const uint8_t *);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vld4q_s16)))
int16x8x4_t vld4q_s16(const int16_t *);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vld4q_s16)))
int16x8x4_t vld4q(const int16_t *);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vld4q_s32)))
int32x4x4_t vld4q_s32(const int32_t *);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vld4q_s32)))
int32x4x4_t vld4q(const int32_t *);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vld4q_s8)))
int8x16x4_t vld4q_s8(const int8_t *);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vld4q_s8)))
int8x16x4_t vld4q(const int8_t *);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vld4q_u16)))
uint16x8x4_t vld4q_u16(const uint16_t *);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vld4q_u16)))
uint16x8x4_t vld4q(const uint16_t *);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vld4q_u32)))
uint32x4x4_t vld4q_u32(const uint32_t *);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vld4q_u32)))
uint32x4x4_t vld4q(const uint32_t *);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vld4q_u8)))
uint8x16x4_t vld4q_u8(const uint8_t *);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vld4q_u8)))
uint8x16x4_t vld4q(const uint8_t *);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vldrbq_gather_offset_s16)))
int16x8_t vldrbq_gather_offset_s16(const int8_t *, uint16x8_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vldrbq_gather_offset_s16)))
int16x8_t vldrbq_gather_offset(const int8_t *, uint16x8_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vldrbq_gather_offset_s32)))
int32x4_t vldrbq_gather_offset_s32(const int8_t *, uint32x4_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vldrbq_gather_offset_s32)))
int32x4_t vldrbq_gather_offset(const int8_t *, uint32x4_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vldrbq_gather_offset_s8)))
int8x16_t vldrbq_gather_offset_s8(const int8_t *, uint8x16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vldrbq_gather_offset_s8)))
int8x16_t vldrbq_gather_offset(const int8_t *, uint8x16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vldrbq_gather_offset_u16)))
uint16x8_t vldrbq_gather_offset_u16(const uint8_t *, uint16x8_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vldrbq_gather_offset_u16)))
uint16x8_t vldrbq_gather_offset(const uint8_t *, uint16x8_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vldrbq_gather_offset_u32)))
uint32x4_t vldrbq_gather_offset_u32(const uint8_t *, uint32x4_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vldrbq_gather_offset_u32)))
uint32x4_t vldrbq_gather_offset(const uint8_t *, uint32x4_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vldrbq_gather_offset_u8)))
uint8x16_t vldrbq_gather_offset_u8(const uint8_t *, uint8x16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vldrbq_gather_offset_u8)))
uint8x16_t vldrbq_gather_offset(const uint8_t *, uint8x16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vldrbq_gather_offset_z_s16)))
int16x8_t vldrbq_gather_offset_z_s16(const int8_t *, uint16x8_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vldrbq_gather_offset_z_s16)))
int16x8_t vldrbq_gather_offset_z(const int8_t *, uint16x8_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vldrbq_gather_offset_z_s32)))
int32x4_t vldrbq_gather_offset_z_s32(const int8_t *, uint32x4_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vldrbq_gather_offset_z_s32)))
int32x4_t vldrbq_gather_offset_z(const int8_t *, uint32x4_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vldrbq_gather_offset_z_s8)))
int8x16_t vldrbq_gather_offset_z_s8(const int8_t *, uint8x16_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vldrbq_gather_offset_z_s8)))
int8x16_t vldrbq_gather_offset_z(const int8_t *, uint8x16_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vldrbq_gather_offset_z_u16)))
uint16x8_t vldrbq_gather_offset_z_u16(const uint8_t *, uint16x8_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vldrbq_gather_offset_z_u16)))
uint16x8_t vldrbq_gather_offset_z(const uint8_t *, uint16x8_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vldrbq_gather_offset_z_u32)))
uint32x4_t vldrbq_gather_offset_z_u32(const uint8_t *, uint32x4_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vldrbq_gather_offset_z_u32)))
uint32x4_t vldrbq_gather_offset_z(const uint8_t *, uint32x4_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vldrbq_gather_offset_z_u8)))
uint8x16_t vldrbq_gather_offset_z_u8(const uint8_t *, uint8x16_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vldrbq_gather_offset_z_u8)))
uint8x16_t vldrbq_gather_offset_z(const uint8_t *, uint8x16_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vldrbq_s16)))
int16x8_t vldrbq_s16(const int8_t *);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vldrbq_s32)))
int32x4_t vldrbq_s32(const int8_t *);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vldrbq_s8)))
int8x16_t vldrbq_s8(const int8_t *);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vldrbq_u16)))
uint16x8_t vldrbq_u16(const uint8_t *);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vldrbq_u32)))
uint32x4_t vldrbq_u32(const uint8_t *);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vldrbq_u8)))
uint8x16_t vldrbq_u8(const uint8_t *);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vldrbq_z_s16)))
int16x8_t vldrbq_z_s16(const int8_t *, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vldrbq_z_s32)))
int32x4_t vldrbq_z_s32(const int8_t *, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vldrbq_z_s8)))
int8x16_t vldrbq_z_s8(const int8_t *, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vldrbq_z_u16)))
uint16x8_t vldrbq_z_u16(const uint8_t *, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vldrbq_z_u32)))
uint32x4_t vldrbq_z_u32(const uint8_t *, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vldrbq_z_u8)))
uint8x16_t vldrbq_z_u8(const uint8_t *, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vldrdq_gather_base_s64)))
int64x2_t vldrdq_gather_base_s64(uint64x2_t, int);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vldrdq_gather_base_u64)))
uint64x2_t vldrdq_gather_base_u64(uint64x2_t, int);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vldrdq_gather_base_wb_s64)))
int64x2_t vldrdq_gather_base_wb_s64(uint64x2_t *, int);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vldrdq_gather_base_wb_u64)))
uint64x2_t vldrdq_gather_base_wb_u64(uint64x2_t *, int);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vldrdq_gather_base_wb_z_s64)))
int64x2_t vldrdq_gather_base_wb_z_s64(uint64x2_t *, int, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vldrdq_gather_base_wb_z_u64)))
uint64x2_t vldrdq_gather_base_wb_z_u64(uint64x2_t *, int, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vldrdq_gather_base_z_s64)))
int64x2_t vldrdq_gather_base_z_s64(uint64x2_t, int, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vldrdq_gather_base_z_u64)))
uint64x2_t vldrdq_gather_base_z_u64(uint64x2_t, int, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vldrdq_gather_offset_s64)))
int64x2_t vldrdq_gather_offset_s64(const int64_t *, uint64x2_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vldrdq_gather_offset_s64)))
int64x2_t vldrdq_gather_offset(const int64_t *, uint64x2_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vldrdq_gather_offset_u64)))
uint64x2_t vldrdq_gather_offset_u64(const uint64_t *, uint64x2_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vldrdq_gather_offset_u64)))
uint64x2_t vldrdq_gather_offset(const uint64_t *, uint64x2_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vldrdq_gather_offset_z_s64)))
int64x2_t vldrdq_gather_offset_z_s64(const int64_t *, uint64x2_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vldrdq_gather_offset_z_s64)))
int64x2_t vldrdq_gather_offset_z(const int64_t *, uint64x2_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vldrdq_gather_offset_z_u64)))
uint64x2_t vldrdq_gather_offset_z_u64(const uint64_t *, uint64x2_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vldrdq_gather_offset_z_u64)))
uint64x2_t vldrdq_gather_offset_z(const uint64_t *, uint64x2_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vldrdq_gather_shifted_offset_s64)))
int64x2_t vldrdq_gather_shifted_offset_s64(const int64_t *, uint64x2_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vldrdq_gather_shifted_offset_s64)))
int64x2_t vldrdq_gather_shifted_offset(const int64_t *, uint64x2_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vldrdq_gather_shifted_offset_u64)))
uint64x2_t vldrdq_gather_shifted_offset_u64(const uint64_t *, uint64x2_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vldrdq_gather_shifted_offset_u64)))
uint64x2_t vldrdq_gather_shifted_offset(const uint64_t *, uint64x2_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vldrdq_gather_shifted_offset_z_s64)))
int64x2_t vldrdq_gather_shifted_offset_z_s64(const int64_t *, uint64x2_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vldrdq_gather_shifted_offset_z_s64)))
int64x2_t vldrdq_gather_shifted_offset_z(const int64_t *, uint64x2_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vldrdq_gather_shifted_offset_z_u64)))
uint64x2_t vldrdq_gather_shifted_offset_z_u64(const uint64_t *, uint64x2_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vldrdq_gather_shifted_offset_z_u64)))
uint64x2_t vldrdq_gather_shifted_offset_z(const uint64_t *, uint64x2_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vldrhq_gather_offset_s16)))
int16x8_t vldrhq_gather_offset_s16(const int16_t *, uint16x8_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vldrhq_gather_offset_s16)))
int16x8_t vldrhq_gather_offset(const int16_t *, uint16x8_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vldrhq_gather_offset_s32)))
int32x4_t vldrhq_gather_offset_s32(const int16_t *, uint32x4_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vldrhq_gather_offset_s32)))
int32x4_t vldrhq_gather_offset(const int16_t *, uint32x4_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vldrhq_gather_offset_u16)))
uint16x8_t vldrhq_gather_offset_u16(const uint16_t *, uint16x8_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vldrhq_gather_offset_u16)))
uint16x8_t vldrhq_gather_offset(const uint16_t *, uint16x8_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vldrhq_gather_offset_u32)))
uint32x4_t vldrhq_gather_offset_u32(const uint16_t *, uint32x4_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vldrhq_gather_offset_u32)))
uint32x4_t vldrhq_gather_offset(const uint16_t *, uint32x4_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vldrhq_gather_offset_z_s16)))
int16x8_t vldrhq_gather_offset_z_s16(const int16_t *, uint16x8_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vldrhq_gather_offset_z_s16)))
int16x8_t vldrhq_gather_offset_z(const int16_t *, uint16x8_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vldrhq_gather_offset_z_s32)))
int32x4_t vldrhq_gather_offset_z_s32(const int16_t *, uint32x4_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vldrhq_gather_offset_z_s32)))
int32x4_t vldrhq_gather_offset_z(const int16_t *, uint32x4_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vldrhq_gather_offset_z_u16)))
uint16x8_t vldrhq_gather_offset_z_u16(const uint16_t *, uint16x8_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vldrhq_gather_offset_z_u16)))
uint16x8_t vldrhq_gather_offset_z(const uint16_t *, uint16x8_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vldrhq_gather_offset_z_u32)))
uint32x4_t vldrhq_gather_offset_z_u32(const uint16_t *, uint32x4_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vldrhq_gather_offset_z_u32)))
uint32x4_t vldrhq_gather_offset_z(const uint16_t *, uint32x4_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vldrhq_gather_shifted_offset_s16)))
int16x8_t vldrhq_gather_shifted_offset_s16(const int16_t *, uint16x8_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vldrhq_gather_shifted_offset_s16)))
int16x8_t vldrhq_gather_shifted_offset(const int16_t *, uint16x8_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vldrhq_gather_shifted_offset_s32)))
int32x4_t vldrhq_gather_shifted_offset_s32(const int16_t *, uint32x4_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vldrhq_gather_shifted_offset_s32)))
int32x4_t vldrhq_gather_shifted_offset(const int16_t *, uint32x4_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vldrhq_gather_shifted_offset_u16)))
uint16x8_t vldrhq_gather_shifted_offset_u16(const uint16_t *, uint16x8_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vldrhq_gather_shifted_offset_u16)))
uint16x8_t vldrhq_gather_shifted_offset(const uint16_t *, uint16x8_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vldrhq_gather_shifted_offset_u32)))
uint32x4_t vldrhq_gather_shifted_offset_u32(const uint16_t *, uint32x4_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vldrhq_gather_shifted_offset_u32)))
uint32x4_t vldrhq_gather_shifted_offset(const uint16_t *, uint32x4_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vldrhq_gather_shifted_offset_z_s16)))
int16x8_t vldrhq_gather_shifted_offset_z_s16(const int16_t *, uint16x8_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vldrhq_gather_shifted_offset_z_s16)))
int16x8_t vldrhq_gather_shifted_offset_z(const int16_t *, uint16x8_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vldrhq_gather_shifted_offset_z_s32)))
int32x4_t vldrhq_gather_shifted_offset_z_s32(const int16_t *, uint32x4_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vldrhq_gather_shifted_offset_z_s32)))
int32x4_t vldrhq_gather_shifted_offset_z(const int16_t *, uint32x4_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vldrhq_gather_shifted_offset_z_u16)))
uint16x8_t vldrhq_gather_shifted_offset_z_u16(const uint16_t *, uint16x8_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vldrhq_gather_shifted_offset_z_u16)))
uint16x8_t vldrhq_gather_shifted_offset_z(const uint16_t *, uint16x8_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vldrhq_gather_shifted_offset_z_u32)))
uint32x4_t vldrhq_gather_shifted_offset_z_u32(const uint16_t *, uint32x4_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vldrhq_gather_shifted_offset_z_u32)))
uint32x4_t vldrhq_gather_shifted_offset_z(const uint16_t *, uint32x4_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vldrhq_s16)))
int16x8_t vldrhq_s16(const int16_t *);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vldrhq_s32)))
int32x4_t vldrhq_s32(const int16_t *);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vldrhq_u16)))
uint16x8_t vldrhq_u16(const uint16_t *);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vldrhq_u32)))
uint32x4_t vldrhq_u32(const uint16_t *);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vldrhq_z_s16)))
int16x8_t vldrhq_z_s16(const int16_t *, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vldrhq_z_s32)))
int32x4_t vldrhq_z_s32(const int16_t *, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vldrhq_z_u16)))
uint16x8_t vldrhq_z_u16(const uint16_t *, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vldrhq_z_u32)))
uint32x4_t vldrhq_z_u32(const uint16_t *, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vldrwq_gather_base_s32)))
int32x4_t vldrwq_gather_base_s32(uint32x4_t, int);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vldrwq_gather_base_u32)))
uint32x4_t vldrwq_gather_base_u32(uint32x4_t, int);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vldrwq_gather_base_wb_s32)))
int32x4_t vldrwq_gather_base_wb_s32(uint32x4_t *, int);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vldrwq_gather_base_wb_u32)))
uint32x4_t vldrwq_gather_base_wb_u32(uint32x4_t *, int);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vldrwq_gather_base_wb_z_s32)))
int32x4_t vldrwq_gather_base_wb_z_s32(uint32x4_t *, int, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vldrwq_gather_base_wb_z_u32)))
uint32x4_t vldrwq_gather_base_wb_z_u32(uint32x4_t *, int, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vldrwq_gather_base_z_s32)))
int32x4_t vldrwq_gather_base_z_s32(uint32x4_t, int, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vldrwq_gather_base_z_u32)))
uint32x4_t vldrwq_gather_base_z_u32(uint32x4_t, int, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vldrwq_gather_offset_s32)))
int32x4_t vldrwq_gather_offset_s32(const int32_t *, uint32x4_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vldrwq_gather_offset_s32)))
int32x4_t vldrwq_gather_offset(const int32_t *, uint32x4_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vldrwq_gather_offset_u32)))
uint32x4_t vldrwq_gather_offset_u32(const uint32_t *, uint32x4_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vldrwq_gather_offset_u32)))
uint32x4_t vldrwq_gather_offset(const uint32_t *, uint32x4_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vldrwq_gather_offset_z_s32)))
int32x4_t vldrwq_gather_offset_z_s32(const int32_t *, uint32x4_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vldrwq_gather_offset_z_s32)))
int32x4_t vldrwq_gather_offset_z(const int32_t *, uint32x4_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vldrwq_gather_offset_z_u32)))
uint32x4_t vldrwq_gather_offset_z_u32(const uint32_t *, uint32x4_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vldrwq_gather_offset_z_u32)))
uint32x4_t vldrwq_gather_offset_z(const uint32_t *, uint32x4_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vldrwq_gather_shifted_offset_s32)))
int32x4_t vldrwq_gather_shifted_offset_s32(const int32_t *, uint32x4_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vldrwq_gather_shifted_offset_s32)))
int32x4_t vldrwq_gather_shifted_offset(const int32_t *, uint32x4_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vldrwq_gather_shifted_offset_u32)))
uint32x4_t vldrwq_gather_shifted_offset_u32(const uint32_t *, uint32x4_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vldrwq_gather_shifted_offset_u32)))
uint32x4_t vldrwq_gather_shifted_offset(const uint32_t *, uint32x4_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vldrwq_gather_shifted_offset_z_s32)))
int32x4_t vldrwq_gather_shifted_offset_z_s32(const int32_t *, uint32x4_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vldrwq_gather_shifted_offset_z_s32)))
int32x4_t vldrwq_gather_shifted_offset_z(const int32_t *, uint32x4_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vldrwq_gather_shifted_offset_z_u32)))
uint32x4_t vldrwq_gather_shifted_offset_z_u32(const uint32_t *, uint32x4_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vldrwq_gather_shifted_offset_z_u32)))
uint32x4_t vldrwq_gather_shifted_offset_z(const uint32_t *, uint32x4_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vldrwq_s32)))
int32x4_t vldrwq_s32(const int32_t *);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vldrwq_u32)))
uint32x4_t vldrwq_u32(const uint32_t *);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vldrwq_z_s32)))
int32x4_t vldrwq_z_s32(const int32_t *, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vldrwq_z_u32)))
uint32x4_t vldrwq_z_u32(const uint32_t *, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vmaxvq_s16)))
int16_t vmaxvq_s16(int16_t, int16x8_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vmaxvq_s16)))
int16_t vmaxvq(int16_t, int16x8_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vmaxvq_s32)))
int32_t vmaxvq_s32(int32_t, int32x4_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vmaxvq_s32)))
int32_t vmaxvq(int32_t, int32x4_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vmaxvq_s8)))
int8_t vmaxvq_s8(int8_t, int8x16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vmaxvq_s8)))
int8_t vmaxvq(int8_t, int8x16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vmaxvq_u16)))
uint16_t vmaxvq_u16(uint16_t, uint16x8_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vmaxvq_u16)))
uint16_t vmaxvq(uint16_t, uint16x8_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vmaxvq_u32)))
uint32_t vmaxvq_u32(uint32_t, uint32x4_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vmaxvq_u32)))
uint32_t vmaxvq(uint32_t, uint32x4_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vmaxvq_u8)))
uint8_t vmaxvq_u8(uint8_t, uint8x16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vmaxvq_u8)))
uint8_t vmaxvq(uint8_t, uint8x16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vminvq_s16)))
int16_t vminvq_s16(int16_t, int16x8_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vminvq_s16)))
int16_t vminvq(int16_t, int16x8_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vminvq_s32)))
int32_t vminvq_s32(int32_t, int32x4_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vminvq_s32)))
int32_t vminvq(int32_t, int32x4_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vminvq_s8)))
int8_t vminvq_s8(int8_t, int8x16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vminvq_s8)))
int8_t vminvq(int8_t, int8x16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vminvq_u16)))
uint16_t vminvq_u16(uint16_t, uint16x8_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vminvq_u16)))
uint16_t vminvq(uint16_t, uint16x8_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vminvq_u32)))
uint32_t vminvq_u32(uint32_t, uint32x4_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vminvq_u32)))
uint32_t vminvq(uint32_t, uint32x4_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vminvq_u8)))
uint8_t vminvq_u8(uint8_t, uint8x16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vminvq_u8)))
uint8_t vminvq(uint8_t, uint8x16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vmulq_m_s16)))
int16x8_t vmulq_m_s16(int16x8_t, int16x8_t, int16x8_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vmulq_m_s16)))
int16x8_t vmulq_m(int16x8_t, int16x8_t, int16x8_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vmulq_m_s32)))
int32x4_t vmulq_m_s32(int32x4_t, int32x4_t, int32x4_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vmulq_m_s32)))
int32x4_t vmulq_m(int32x4_t, int32x4_t, int32x4_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vmulq_m_s8)))
int8x16_t vmulq_m_s8(int8x16_t, int8x16_t, int8x16_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vmulq_m_s8)))
int8x16_t vmulq_m(int8x16_t, int8x16_t, int8x16_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vmulq_m_u16)))
uint16x8_t vmulq_m_u16(uint16x8_t, uint16x8_t, uint16x8_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vmulq_m_u16)))
uint16x8_t vmulq_m(uint16x8_t, uint16x8_t, uint16x8_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vmulq_m_u32)))
uint32x4_t vmulq_m_u32(uint32x4_t, uint32x4_t, uint32x4_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vmulq_m_u32)))
uint32x4_t vmulq_m(uint32x4_t, uint32x4_t, uint32x4_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vmulq_m_u8)))
uint8x16_t vmulq_m_u8(uint8x16_t, uint8x16_t, uint8x16_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vmulq_m_u8)))
uint8x16_t vmulq_m(uint8x16_t, uint8x16_t, uint8x16_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vmulq_s16)))
int16x8_t vmulq_s16(int16x8_t, int16x8_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vmulq_s16)))
int16x8_t vmulq(int16x8_t, int16x8_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vmulq_s32)))
int32x4_t vmulq_s32(int32x4_t, int32x4_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vmulq_s32)))
int32x4_t vmulq(int32x4_t, int32x4_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vmulq_s8)))
int8x16_t vmulq_s8(int8x16_t, int8x16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vmulq_s8)))
int8x16_t vmulq(int8x16_t, int8x16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vmulq_u16)))
uint16x8_t vmulq_u16(uint16x8_t, uint16x8_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vmulq_u16)))
uint16x8_t vmulq(uint16x8_t, uint16x8_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vmulq_u32)))
uint32x4_t vmulq_u32(uint32x4_t, uint32x4_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vmulq_u32)))
uint32x4_t vmulq(uint32x4_t, uint32x4_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vmulq_u8)))
uint8x16_t vmulq_u8(uint8x16_t, uint8x16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vmulq_u8)))
uint8x16_t vmulq(uint8x16_t, uint8x16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vornq_m_s16)))
int16x8_t vornq_m_s16(int16x8_t, int16x8_t, int16x8_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vornq_m_s16)))
int16x8_t vornq_m(int16x8_t, int16x8_t, int16x8_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vornq_m_s32)))
int32x4_t vornq_m_s32(int32x4_t, int32x4_t, int32x4_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vornq_m_s32)))
int32x4_t vornq_m(int32x4_t, int32x4_t, int32x4_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vornq_m_s8)))
int8x16_t vornq_m_s8(int8x16_t, int8x16_t, int8x16_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vornq_m_s8)))
int8x16_t vornq_m(int8x16_t, int8x16_t, int8x16_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vornq_m_u16)))
uint16x8_t vornq_m_u16(uint16x8_t, uint16x8_t, uint16x8_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vornq_m_u16)))
uint16x8_t vornq_m(uint16x8_t, uint16x8_t, uint16x8_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vornq_m_u32)))
uint32x4_t vornq_m_u32(uint32x4_t, uint32x4_t, uint32x4_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vornq_m_u32)))
uint32x4_t vornq_m(uint32x4_t, uint32x4_t, uint32x4_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vornq_m_u8)))
uint8x16_t vornq_m_u8(uint8x16_t, uint8x16_t, uint8x16_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vornq_m_u8)))
uint8x16_t vornq_m(uint8x16_t, uint8x16_t, uint8x16_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vornq_s16)))
int16x8_t vornq_s16(int16x8_t, int16x8_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vornq_s16)))
int16x8_t vornq(int16x8_t, int16x8_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vornq_s32)))
int32x4_t vornq_s32(int32x4_t, int32x4_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vornq_s32)))
int32x4_t vornq(int32x4_t, int32x4_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vornq_s8)))
int8x16_t vornq_s8(int8x16_t, int8x16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vornq_s8)))
int8x16_t vornq(int8x16_t, int8x16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vornq_u16)))
uint16x8_t vornq_u16(uint16x8_t, uint16x8_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vornq_u16)))
uint16x8_t vornq(uint16x8_t, uint16x8_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vornq_u32)))
uint32x4_t vornq_u32(uint32x4_t, uint32x4_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vornq_u32)))
uint32x4_t vornq(uint32x4_t, uint32x4_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vornq_u8)))
uint8x16_t vornq_u8(uint8x16_t, uint8x16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vornq_u8)))
uint8x16_t vornq(uint8x16_t, uint8x16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vorrq_m_s16)))
int16x8_t vorrq_m_s16(int16x8_t, int16x8_t, int16x8_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vorrq_m_s16)))
int16x8_t vorrq_m(int16x8_t, int16x8_t, int16x8_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vorrq_m_s32)))
int32x4_t vorrq_m_s32(int32x4_t, int32x4_t, int32x4_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vorrq_m_s32)))
int32x4_t vorrq_m(int32x4_t, int32x4_t, int32x4_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vorrq_m_s8)))
int8x16_t vorrq_m_s8(int8x16_t, int8x16_t, int8x16_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vorrq_m_s8)))
int8x16_t vorrq_m(int8x16_t, int8x16_t, int8x16_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vorrq_m_u16)))
uint16x8_t vorrq_m_u16(uint16x8_t, uint16x8_t, uint16x8_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vorrq_m_u16)))
uint16x8_t vorrq_m(uint16x8_t, uint16x8_t, uint16x8_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vorrq_m_u32)))
uint32x4_t vorrq_m_u32(uint32x4_t, uint32x4_t, uint32x4_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vorrq_m_u32)))
uint32x4_t vorrq_m(uint32x4_t, uint32x4_t, uint32x4_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vorrq_m_u8)))
uint8x16_t vorrq_m_u8(uint8x16_t, uint8x16_t, uint8x16_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vorrq_m_u8)))
uint8x16_t vorrq_m(uint8x16_t, uint8x16_t, uint8x16_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vorrq_s16)))
int16x8_t vorrq_s16(int16x8_t, int16x8_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vorrq_s16)))
int16x8_t vorrq(int16x8_t, int16x8_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vorrq_s32)))
int32x4_t vorrq_s32(int32x4_t, int32x4_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vorrq_s32)))
int32x4_t vorrq(int32x4_t, int32x4_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vorrq_s8)))
int8x16_t vorrq_s8(int8x16_t, int8x16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vorrq_s8)))
int8x16_t vorrq(int8x16_t, int8x16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vorrq_u16)))
uint16x8_t vorrq_u16(uint16x8_t, uint16x8_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vorrq_u16)))
uint16x8_t vorrq(uint16x8_t, uint16x8_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vorrq_u32)))
uint32x4_t vorrq_u32(uint32x4_t, uint32x4_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vorrq_u32)))
uint32x4_t vorrq(uint32x4_t, uint32x4_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vorrq_u8)))
uint8x16_t vorrq_u8(uint8x16_t, uint8x16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vorrq_u8)))
uint8x16_t vorrq(uint8x16_t, uint8x16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_s16_s32)))
int16x8_t vreinterpretq_s16_s32(int32x4_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_s16_s32)))
int16x8_t vreinterpretq_s16(int32x4_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_s16_s64)))
int16x8_t vreinterpretq_s16_s64(int64x2_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_s16_s64)))
int16x8_t vreinterpretq_s16(int64x2_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_s16_s8)))
int16x8_t vreinterpretq_s16_s8(int8x16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_s16_s8)))
int16x8_t vreinterpretq_s16(int8x16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_s16_u16)))
int16x8_t vreinterpretq_s16_u16(uint16x8_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_s16_u16)))
int16x8_t vreinterpretq_s16(uint16x8_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_s16_u32)))
int16x8_t vreinterpretq_s16_u32(uint32x4_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_s16_u32)))
int16x8_t vreinterpretq_s16(uint32x4_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_s16_u64)))
int16x8_t vreinterpretq_s16_u64(uint64x2_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_s16_u64)))
int16x8_t vreinterpretq_s16(uint64x2_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_s16_u8)))
int16x8_t vreinterpretq_s16_u8(uint8x16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_s16_u8)))
int16x8_t vreinterpretq_s16(uint8x16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_s32_s16)))
int32x4_t vreinterpretq_s32_s16(int16x8_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_s32_s16)))
int32x4_t vreinterpretq_s32(int16x8_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_s32_s64)))
int32x4_t vreinterpretq_s32_s64(int64x2_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_s32_s64)))
int32x4_t vreinterpretq_s32(int64x2_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_s32_s8)))
int32x4_t vreinterpretq_s32_s8(int8x16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_s32_s8)))
int32x4_t vreinterpretq_s32(int8x16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_s32_u16)))
int32x4_t vreinterpretq_s32_u16(uint16x8_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_s32_u16)))
int32x4_t vreinterpretq_s32(uint16x8_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_s32_u32)))
int32x4_t vreinterpretq_s32_u32(uint32x4_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_s32_u32)))
int32x4_t vreinterpretq_s32(uint32x4_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_s32_u64)))
int32x4_t vreinterpretq_s32_u64(uint64x2_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_s32_u64)))
int32x4_t vreinterpretq_s32(uint64x2_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_s32_u8)))
int32x4_t vreinterpretq_s32_u8(uint8x16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_s32_u8)))
int32x4_t vreinterpretq_s32(uint8x16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_s64_s16)))
int64x2_t vreinterpretq_s64_s16(int16x8_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_s64_s16)))
int64x2_t vreinterpretq_s64(int16x8_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_s64_s32)))
int64x2_t vreinterpretq_s64_s32(int32x4_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_s64_s32)))
int64x2_t vreinterpretq_s64(int32x4_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_s64_s8)))
int64x2_t vreinterpretq_s64_s8(int8x16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_s64_s8)))
int64x2_t vreinterpretq_s64(int8x16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_s64_u16)))
int64x2_t vreinterpretq_s64_u16(uint16x8_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_s64_u16)))
int64x2_t vreinterpretq_s64(uint16x8_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_s64_u32)))
int64x2_t vreinterpretq_s64_u32(uint32x4_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_s64_u32)))
int64x2_t vreinterpretq_s64(uint32x4_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_s64_u64)))
int64x2_t vreinterpretq_s64_u64(uint64x2_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_s64_u64)))
int64x2_t vreinterpretq_s64(uint64x2_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_s64_u8)))
int64x2_t vreinterpretq_s64_u8(uint8x16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_s64_u8)))
int64x2_t vreinterpretq_s64(uint8x16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_s8_s16)))
int8x16_t vreinterpretq_s8_s16(int16x8_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_s8_s16)))
int8x16_t vreinterpretq_s8(int16x8_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_s8_s32)))
int8x16_t vreinterpretq_s8_s32(int32x4_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_s8_s32)))
int8x16_t vreinterpretq_s8(int32x4_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_s8_s64)))
int8x16_t vreinterpretq_s8_s64(int64x2_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_s8_s64)))
int8x16_t vreinterpretq_s8(int64x2_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_s8_u16)))
int8x16_t vreinterpretq_s8_u16(uint16x8_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_s8_u16)))
int8x16_t vreinterpretq_s8(uint16x8_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_s8_u32)))
int8x16_t vreinterpretq_s8_u32(uint32x4_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_s8_u32)))
int8x16_t vreinterpretq_s8(uint32x4_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_s8_u64)))
int8x16_t vreinterpretq_s8_u64(uint64x2_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_s8_u64)))
int8x16_t vreinterpretq_s8(uint64x2_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_s8_u8)))
int8x16_t vreinterpretq_s8_u8(uint8x16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_s8_u8)))
int8x16_t vreinterpretq_s8(uint8x16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_u16_s16)))
uint16x8_t vreinterpretq_u16_s16(int16x8_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_u16_s16)))
uint16x8_t vreinterpretq_u16(int16x8_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_u16_s32)))
uint16x8_t vreinterpretq_u16_s32(int32x4_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_u16_s32)))
uint16x8_t vreinterpretq_u16(int32x4_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_u16_s64)))
uint16x8_t vreinterpretq_u16_s64(int64x2_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_u16_s64)))
uint16x8_t vreinterpretq_u16(int64x2_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_u16_s8)))
uint16x8_t vreinterpretq_u16_s8(int8x16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_u16_s8)))
uint16x8_t vreinterpretq_u16(int8x16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_u16_u32)))
uint16x8_t vreinterpretq_u16_u32(uint32x4_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_u16_u32)))
uint16x8_t vreinterpretq_u16(uint32x4_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_u16_u64)))
uint16x8_t vreinterpretq_u16_u64(uint64x2_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_u16_u64)))
uint16x8_t vreinterpretq_u16(uint64x2_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_u16_u8)))
uint16x8_t vreinterpretq_u16_u8(uint8x16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_u16_u8)))
uint16x8_t vreinterpretq_u16(uint8x16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_u32_s16)))
uint32x4_t vreinterpretq_u32_s16(int16x8_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_u32_s16)))
uint32x4_t vreinterpretq_u32(int16x8_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_u32_s32)))
uint32x4_t vreinterpretq_u32_s32(int32x4_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_u32_s32)))
uint32x4_t vreinterpretq_u32(int32x4_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_u32_s64)))
uint32x4_t vreinterpretq_u32_s64(int64x2_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_u32_s64)))
uint32x4_t vreinterpretq_u32(int64x2_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_u32_s8)))
uint32x4_t vreinterpretq_u32_s8(int8x16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_u32_s8)))
uint32x4_t vreinterpretq_u32(int8x16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_u32_u16)))
uint32x4_t vreinterpretq_u32_u16(uint16x8_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_u32_u16)))
uint32x4_t vreinterpretq_u32(uint16x8_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_u32_u64)))
uint32x4_t vreinterpretq_u32_u64(uint64x2_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_u32_u64)))
uint32x4_t vreinterpretq_u32(uint64x2_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_u32_u8)))
uint32x4_t vreinterpretq_u32_u8(uint8x16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_u32_u8)))
uint32x4_t vreinterpretq_u32(uint8x16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_u64_s16)))
uint64x2_t vreinterpretq_u64_s16(int16x8_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_u64_s16)))
uint64x2_t vreinterpretq_u64(int16x8_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_u64_s32)))
uint64x2_t vreinterpretq_u64_s32(int32x4_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_u64_s32)))
uint64x2_t vreinterpretq_u64(int32x4_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_u64_s64)))
uint64x2_t vreinterpretq_u64_s64(int64x2_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_u64_s64)))
uint64x2_t vreinterpretq_u64(int64x2_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_u64_s8)))
uint64x2_t vreinterpretq_u64_s8(int8x16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_u64_s8)))
uint64x2_t vreinterpretq_u64(int8x16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_u64_u16)))
uint64x2_t vreinterpretq_u64_u16(uint16x8_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_u64_u16)))
uint64x2_t vreinterpretq_u64(uint16x8_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_u64_u32)))
uint64x2_t vreinterpretq_u64_u32(uint32x4_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_u64_u32)))
uint64x2_t vreinterpretq_u64(uint32x4_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_u64_u8)))
uint64x2_t vreinterpretq_u64_u8(uint8x16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_u64_u8)))
uint64x2_t vreinterpretq_u64(uint8x16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_u8_s16)))
uint8x16_t vreinterpretq_u8_s16(int16x8_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_u8_s16)))
uint8x16_t vreinterpretq_u8(int16x8_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_u8_s32)))
uint8x16_t vreinterpretq_u8_s32(int32x4_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_u8_s32)))
uint8x16_t vreinterpretq_u8(int32x4_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_u8_s64)))
uint8x16_t vreinterpretq_u8_s64(int64x2_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_u8_s64)))
uint8x16_t vreinterpretq_u8(int64x2_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_u8_s8)))
uint8x16_t vreinterpretq_u8_s8(int8x16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_u8_s8)))
uint8x16_t vreinterpretq_u8(int8x16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_u8_u16)))
uint8x16_t vreinterpretq_u8_u16(uint16x8_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_u8_u16)))
uint8x16_t vreinterpretq_u8(uint16x8_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_u8_u32)))
uint8x16_t vreinterpretq_u8_u32(uint32x4_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_u8_u32)))
uint8x16_t vreinterpretq_u8(uint32x4_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_u8_u64)))
uint8x16_t vreinterpretq_u8_u64(uint64x2_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_u8_u64)))
uint8x16_t vreinterpretq_u8(uint64x2_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vsetq_lane_s16)))
int16x8_t vsetq_lane_s16(int16_t, int16x8_t, int);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vsetq_lane_s16)))
int16x8_t vsetq_lane(int16_t, int16x8_t, int);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vsetq_lane_s32)))
int32x4_t vsetq_lane_s32(int32_t, int32x4_t, int);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vsetq_lane_s32)))
int32x4_t vsetq_lane(int32_t, int32x4_t, int);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vsetq_lane_s64)))
int64x2_t vsetq_lane_s64(int64_t, int64x2_t, int);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vsetq_lane_s64)))
int64x2_t vsetq_lane(int64_t, int64x2_t, int);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vsetq_lane_s8)))
int8x16_t vsetq_lane_s8(int8_t, int8x16_t, int);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vsetq_lane_s8)))
int8x16_t vsetq_lane(int8_t, int8x16_t, int);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vsetq_lane_u16)))
uint16x8_t vsetq_lane_u16(uint16_t, uint16x8_t, int);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vsetq_lane_u16)))
uint16x8_t vsetq_lane(uint16_t, uint16x8_t, int);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vsetq_lane_u32)))
uint32x4_t vsetq_lane_u32(uint32_t, uint32x4_t, int);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vsetq_lane_u32)))
uint32x4_t vsetq_lane(uint32_t, uint32x4_t, int);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vsetq_lane_u64)))
uint64x2_t vsetq_lane_u64(uint64_t, uint64x2_t, int);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vsetq_lane_u64)))
uint64x2_t vsetq_lane(uint64_t, uint64x2_t, int);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vsetq_lane_u8)))
uint8x16_t vsetq_lane_u8(uint8_t, uint8x16_t, int);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vsetq_lane_u8)))
uint8x16_t vsetq_lane(uint8_t, uint8x16_t, int);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vst1q_p_s16)))
void vst1q_p_s16(int16_t *, int16x8_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vst1q_p_s16)))
void vst1q_p(int16_t *, int16x8_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vst1q_p_s32)))
void vst1q_p_s32(int32_t *, int32x4_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vst1q_p_s32)))
void vst1q_p(int32_t *, int32x4_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vst1q_p_s8)))
void vst1q_p_s8(int8_t *, int8x16_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vst1q_p_s8)))
void vst1q_p(int8_t *, int8x16_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vst1q_p_u16)))
void vst1q_p_u16(uint16_t *, uint16x8_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vst1q_p_u16)))
void vst1q_p(uint16_t *, uint16x8_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vst1q_p_u32)))
void vst1q_p_u32(uint32_t *, uint32x4_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vst1q_p_u32)))
void vst1q_p(uint32_t *, uint32x4_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vst1q_p_u8)))
void vst1q_p_u8(uint8_t *, uint8x16_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vst1q_p_u8)))
void vst1q_p(uint8_t *, uint8x16_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vst1q_s16)))
void vst1q_s16(int16_t *, int16x8_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vst1q_s16)))
void vst1q(int16_t *, int16x8_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vst1q_s32)))
void vst1q_s32(int32_t *, int32x4_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vst1q_s32)))
void vst1q(int32_t *, int32x4_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vst1q_s8)))
void vst1q_s8(int8_t *, int8x16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vst1q_s8)))
void vst1q(int8_t *, int8x16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vst1q_u16)))
void vst1q_u16(uint16_t *, uint16x8_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vst1q_u16)))
void vst1q(uint16_t *, uint16x8_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vst1q_u32)))
void vst1q_u32(uint32_t *, uint32x4_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vst1q_u32)))
void vst1q(uint32_t *, uint32x4_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vst1q_u8)))
void vst1q_u8(uint8_t *, uint8x16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vst1q_u8)))
void vst1q(uint8_t *, uint8x16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vst2q_s16)))
void vst2q_s16(int16_t *, int16x8x2_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vst2q_s16)))
void vst2q(int16_t *, int16x8x2_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vst2q_s32)))
void vst2q_s32(int32_t *, int32x4x2_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vst2q_s32)))
void vst2q(int32_t *, int32x4x2_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vst2q_s8)))
void vst2q_s8(int8_t *, int8x16x2_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vst2q_s8)))
void vst2q(int8_t *, int8x16x2_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vst2q_u16)))
void vst2q_u16(uint16_t *, uint16x8x2_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vst2q_u16)))
void vst2q(uint16_t *, uint16x8x2_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vst2q_u32)))
void vst2q_u32(uint32_t *, uint32x4x2_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vst2q_u32)))
void vst2q(uint32_t *, uint32x4x2_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vst2q_u8)))
void vst2q_u8(uint8_t *, uint8x16x2_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vst2q_u8)))
void vst2q(uint8_t *, uint8x16x2_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vst4q_s16)))
void vst4q_s16(int16_t *, int16x8x4_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vst4q_s16)))
void vst4q(int16_t *, int16x8x4_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vst4q_s32)))
void vst4q_s32(int32_t *, int32x4x4_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vst4q_s32)))
void vst4q(int32_t *, int32x4x4_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vst4q_s8)))
void vst4q_s8(int8_t *, int8x16x4_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vst4q_s8)))
void vst4q(int8_t *, int8x16x4_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vst4q_u16)))
void vst4q_u16(uint16_t *, uint16x8x4_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vst4q_u16)))
void vst4q(uint16_t *, uint16x8x4_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vst4q_u32)))
void vst4q_u32(uint32_t *, uint32x4x4_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vst4q_u32)))
void vst4q(uint32_t *, uint32x4x4_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vst4q_u8)))
void vst4q_u8(uint8_t *, uint8x16x4_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vst4q_u8)))
void vst4q(uint8_t *, uint8x16x4_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vstrbq_p_s16)))
void vstrbq_p_s16(int8_t *, int16x8_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vstrbq_p_s16)))
void vstrbq_p(int8_t *, int16x8_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vstrbq_p_s32)))
void vstrbq_p_s32(int8_t *, int32x4_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vstrbq_p_s32)))
void vstrbq_p(int8_t *, int32x4_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vstrbq_p_s8)))
void vstrbq_p_s8(int8_t *, int8x16_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vstrbq_p_s8)))
void vstrbq_p(int8_t *, int8x16_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vstrbq_p_u16)))
void vstrbq_p_u16(uint8_t *, uint16x8_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vstrbq_p_u16)))
void vstrbq_p(uint8_t *, uint16x8_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vstrbq_p_u32)))
void vstrbq_p_u32(uint8_t *, uint32x4_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vstrbq_p_u32)))
void vstrbq_p(uint8_t *, uint32x4_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vstrbq_p_u8)))
void vstrbq_p_u8(uint8_t *, uint8x16_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vstrbq_p_u8)))
void vstrbq_p(uint8_t *, uint8x16_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vstrbq_s16)))
void vstrbq_s16(int8_t *, int16x8_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vstrbq_s16)))
void vstrbq(int8_t *, int16x8_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vstrbq_s32)))
void vstrbq_s32(int8_t *, int32x4_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vstrbq_s32)))
void vstrbq(int8_t *, int32x4_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vstrbq_s8)))
void vstrbq_s8(int8_t *, int8x16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vstrbq_s8)))
void vstrbq(int8_t *, int8x16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vstrbq_scatter_offset_p_s16)))
void vstrbq_scatter_offset_p_s16(int8_t *, uint16x8_t, int16x8_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vstrbq_scatter_offset_p_s16)))
void vstrbq_scatter_offset_p(int8_t *, uint16x8_t, int16x8_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vstrbq_scatter_offset_p_s32)))
void vstrbq_scatter_offset_p_s32(int8_t *, uint32x4_t, int32x4_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vstrbq_scatter_offset_p_s32)))
void vstrbq_scatter_offset_p(int8_t *, uint32x4_t, int32x4_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vstrbq_scatter_offset_p_s8)))
void vstrbq_scatter_offset_p_s8(int8_t *, uint8x16_t, int8x16_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vstrbq_scatter_offset_p_s8)))
void vstrbq_scatter_offset_p(int8_t *, uint8x16_t, int8x16_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vstrbq_scatter_offset_p_u16)))
void vstrbq_scatter_offset_p_u16(uint8_t *, uint16x8_t, uint16x8_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vstrbq_scatter_offset_p_u16)))
void vstrbq_scatter_offset_p(uint8_t *, uint16x8_t, uint16x8_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vstrbq_scatter_offset_p_u32)))
void vstrbq_scatter_offset_p_u32(uint8_t *, uint32x4_t, uint32x4_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vstrbq_scatter_offset_p_u32)))
void vstrbq_scatter_offset_p(uint8_t *, uint32x4_t, uint32x4_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vstrbq_scatter_offset_p_u8)))
void vstrbq_scatter_offset_p_u8(uint8_t *, uint8x16_t, uint8x16_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vstrbq_scatter_offset_p_u8)))
void vstrbq_scatter_offset_p(uint8_t *, uint8x16_t, uint8x16_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vstrbq_scatter_offset_s16)))
void vstrbq_scatter_offset_s16(int8_t *, uint16x8_t, int16x8_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vstrbq_scatter_offset_s16)))
void vstrbq_scatter_offset(int8_t *, uint16x8_t, int16x8_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vstrbq_scatter_offset_s32)))
void vstrbq_scatter_offset_s32(int8_t *, uint32x4_t, int32x4_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vstrbq_scatter_offset_s32)))
void vstrbq_scatter_offset(int8_t *, uint32x4_t, int32x4_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vstrbq_scatter_offset_s8)))
void vstrbq_scatter_offset_s8(int8_t *, uint8x16_t, int8x16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vstrbq_scatter_offset_s8)))
void vstrbq_scatter_offset(int8_t *, uint8x16_t, int8x16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vstrbq_scatter_offset_u16)))
void vstrbq_scatter_offset_u16(uint8_t *, uint16x8_t, uint16x8_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vstrbq_scatter_offset_u16)))
void vstrbq_scatter_offset(uint8_t *, uint16x8_t, uint16x8_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vstrbq_scatter_offset_u32)))
void vstrbq_scatter_offset_u32(uint8_t *, uint32x4_t, uint32x4_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vstrbq_scatter_offset_u32)))
void vstrbq_scatter_offset(uint8_t *, uint32x4_t, uint32x4_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vstrbq_scatter_offset_u8)))
void vstrbq_scatter_offset_u8(uint8_t *, uint8x16_t, uint8x16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vstrbq_scatter_offset_u8)))
void vstrbq_scatter_offset(uint8_t *, uint8x16_t, uint8x16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vstrbq_u16)))
void vstrbq_u16(uint8_t *, uint16x8_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vstrbq_u16)))
void vstrbq(uint8_t *, uint16x8_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vstrbq_u32)))
void vstrbq_u32(uint8_t *, uint32x4_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vstrbq_u32)))
void vstrbq(uint8_t *, uint32x4_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vstrbq_u8)))
void vstrbq_u8(uint8_t *, uint8x16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vstrbq_u8)))
void vstrbq(uint8_t *, uint8x16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vstrdq_scatter_base_p_s64)))
void vstrdq_scatter_base_p_s64(uint64x2_t, int, int64x2_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vstrdq_scatter_base_p_s64)))
void vstrdq_scatter_base_p(uint64x2_t, int, int64x2_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vstrdq_scatter_base_p_u64)))
void vstrdq_scatter_base_p_u64(uint64x2_t, int, uint64x2_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vstrdq_scatter_base_p_u64)))
void vstrdq_scatter_base_p(uint64x2_t, int, uint64x2_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vstrdq_scatter_base_s64)))
void vstrdq_scatter_base_s64(uint64x2_t, int, int64x2_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vstrdq_scatter_base_s64)))
void vstrdq_scatter_base(uint64x2_t, int, int64x2_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vstrdq_scatter_base_u64)))
void vstrdq_scatter_base_u64(uint64x2_t, int, uint64x2_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vstrdq_scatter_base_u64)))
void vstrdq_scatter_base(uint64x2_t, int, uint64x2_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vstrdq_scatter_base_wb_p_s64)))
void vstrdq_scatter_base_wb_p_s64(uint64x2_t *, int, int64x2_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vstrdq_scatter_base_wb_p_s64)))
void vstrdq_scatter_base_wb_p(uint64x2_t *, int, int64x2_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vstrdq_scatter_base_wb_p_u64)))
void vstrdq_scatter_base_wb_p_u64(uint64x2_t *, int, uint64x2_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vstrdq_scatter_base_wb_p_u64)))
void vstrdq_scatter_base_wb_p(uint64x2_t *, int, uint64x2_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vstrdq_scatter_base_wb_s64)))
void vstrdq_scatter_base_wb_s64(uint64x2_t *, int, int64x2_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vstrdq_scatter_base_wb_s64)))
void vstrdq_scatter_base_wb(uint64x2_t *, int, int64x2_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vstrdq_scatter_base_wb_u64)))
void vstrdq_scatter_base_wb_u64(uint64x2_t *, int, uint64x2_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vstrdq_scatter_base_wb_u64)))
void vstrdq_scatter_base_wb(uint64x2_t *, int, uint64x2_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vstrdq_scatter_offset_p_s64)))
void vstrdq_scatter_offset_p_s64(int64_t *, uint64x2_t, int64x2_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vstrdq_scatter_offset_p_s64)))
void vstrdq_scatter_offset_p(int64_t *, uint64x2_t, int64x2_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vstrdq_scatter_offset_p_u64)))
void vstrdq_scatter_offset_p_u64(uint64_t *, uint64x2_t, uint64x2_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vstrdq_scatter_offset_p_u64)))
void vstrdq_scatter_offset_p(uint64_t *, uint64x2_t, uint64x2_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vstrdq_scatter_offset_s64)))
void vstrdq_scatter_offset_s64(int64_t *, uint64x2_t, int64x2_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vstrdq_scatter_offset_s64)))
void vstrdq_scatter_offset(int64_t *, uint64x2_t, int64x2_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vstrdq_scatter_offset_u64)))
void vstrdq_scatter_offset_u64(uint64_t *, uint64x2_t, uint64x2_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vstrdq_scatter_offset_u64)))
void vstrdq_scatter_offset(uint64_t *, uint64x2_t, uint64x2_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vstrdq_scatter_shifted_offset_p_s64)))
void vstrdq_scatter_shifted_offset_p_s64(int64_t *, uint64x2_t, int64x2_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vstrdq_scatter_shifted_offset_p_s64)))
void vstrdq_scatter_shifted_offset_p(int64_t *, uint64x2_t, int64x2_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vstrdq_scatter_shifted_offset_p_u64)))
void vstrdq_scatter_shifted_offset_p_u64(uint64_t *, uint64x2_t, uint64x2_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vstrdq_scatter_shifted_offset_p_u64)))
void vstrdq_scatter_shifted_offset_p(uint64_t *, uint64x2_t, uint64x2_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vstrdq_scatter_shifted_offset_s64)))
void vstrdq_scatter_shifted_offset_s64(int64_t *, uint64x2_t, int64x2_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vstrdq_scatter_shifted_offset_s64)))
void vstrdq_scatter_shifted_offset(int64_t *, uint64x2_t, int64x2_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vstrdq_scatter_shifted_offset_u64)))
void vstrdq_scatter_shifted_offset_u64(uint64_t *, uint64x2_t, uint64x2_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vstrdq_scatter_shifted_offset_u64)))
void vstrdq_scatter_shifted_offset(uint64_t *, uint64x2_t, uint64x2_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vstrhq_p_s16)))
void vstrhq_p_s16(int16_t *, int16x8_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vstrhq_p_s16)))
void vstrhq_p(int16_t *, int16x8_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vstrhq_p_s32)))
void vstrhq_p_s32(int16_t *, int32x4_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vstrhq_p_s32)))
void vstrhq_p(int16_t *, int32x4_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vstrhq_p_u16)))
void vstrhq_p_u16(uint16_t *, uint16x8_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vstrhq_p_u16)))
void vstrhq_p(uint16_t *, uint16x8_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vstrhq_p_u32)))
void vstrhq_p_u32(uint16_t *, uint32x4_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vstrhq_p_u32)))
void vstrhq_p(uint16_t *, uint32x4_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vstrhq_s16)))
void vstrhq_s16(int16_t *, int16x8_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vstrhq_s16)))
void vstrhq(int16_t *, int16x8_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vstrhq_s32)))
void vstrhq_s32(int16_t *, int32x4_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vstrhq_s32)))
void vstrhq(int16_t *, int32x4_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vstrhq_scatter_offset_p_s16)))
void vstrhq_scatter_offset_p_s16(int16_t *, uint16x8_t, int16x8_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vstrhq_scatter_offset_p_s16)))
void vstrhq_scatter_offset_p(int16_t *, uint16x8_t, int16x8_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vstrhq_scatter_offset_p_s32)))
void vstrhq_scatter_offset_p_s32(int16_t *, uint32x4_t, int32x4_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vstrhq_scatter_offset_p_s32)))
void vstrhq_scatter_offset_p(int16_t *, uint32x4_t, int32x4_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vstrhq_scatter_offset_p_u16)))
void vstrhq_scatter_offset_p_u16(uint16_t *, uint16x8_t, uint16x8_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vstrhq_scatter_offset_p_u16)))
void vstrhq_scatter_offset_p(uint16_t *, uint16x8_t, uint16x8_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vstrhq_scatter_offset_p_u32)))
void vstrhq_scatter_offset_p_u32(uint16_t *, uint32x4_t, uint32x4_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vstrhq_scatter_offset_p_u32)))
void vstrhq_scatter_offset_p(uint16_t *, uint32x4_t, uint32x4_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vstrhq_scatter_offset_s16)))
void vstrhq_scatter_offset_s16(int16_t *, uint16x8_t, int16x8_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vstrhq_scatter_offset_s16)))
void vstrhq_scatter_offset(int16_t *, uint16x8_t, int16x8_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vstrhq_scatter_offset_s32)))
void vstrhq_scatter_offset_s32(int16_t *, uint32x4_t, int32x4_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vstrhq_scatter_offset_s32)))
void vstrhq_scatter_offset(int16_t *, uint32x4_t, int32x4_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vstrhq_scatter_offset_u16)))
void vstrhq_scatter_offset_u16(uint16_t *, uint16x8_t, uint16x8_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vstrhq_scatter_offset_u16)))
void vstrhq_scatter_offset(uint16_t *, uint16x8_t, uint16x8_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vstrhq_scatter_offset_u32)))
void vstrhq_scatter_offset_u32(uint16_t *, uint32x4_t, uint32x4_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vstrhq_scatter_offset_u32)))
void vstrhq_scatter_offset(uint16_t *, uint32x4_t, uint32x4_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vstrhq_scatter_shifted_offset_p_s16)))
void vstrhq_scatter_shifted_offset_p_s16(int16_t *, uint16x8_t, int16x8_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vstrhq_scatter_shifted_offset_p_s16)))
void vstrhq_scatter_shifted_offset_p(int16_t *, uint16x8_t, int16x8_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vstrhq_scatter_shifted_offset_p_s32)))
void vstrhq_scatter_shifted_offset_p_s32(int16_t *, uint32x4_t, int32x4_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vstrhq_scatter_shifted_offset_p_s32)))
void vstrhq_scatter_shifted_offset_p(int16_t *, uint32x4_t, int32x4_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vstrhq_scatter_shifted_offset_p_u16)))
void vstrhq_scatter_shifted_offset_p_u16(uint16_t *, uint16x8_t, uint16x8_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vstrhq_scatter_shifted_offset_p_u16)))
void vstrhq_scatter_shifted_offset_p(uint16_t *, uint16x8_t, uint16x8_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vstrhq_scatter_shifted_offset_p_u32)))
void vstrhq_scatter_shifted_offset_p_u32(uint16_t *, uint32x4_t, uint32x4_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vstrhq_scatter_shifted_offset_p_u32)))
void vstrhq_scatter_shifted_offset_p(uint16_t *, uint32x4_t, uint32x4_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vstrhq_scatter_shifted_offset_s16)))
void vstrhq_scatter_shifted_offset_s16(int16_t *, uint16x8_t, int16x8_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vstrhq_scatter_shifted_offset_s16)))
void vstrhq_scatter_shifted_offset(int16_t *, uint16x8_t, int16x8_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vstrhq_scatter_shifted_offset_s32)))
void vstrhq_scatter_shifted_offset_s32(int16_t *, uint32x4_t, int32x4_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vstrhq_scatter_shifted_offset_s32)))
void vstrhq_scatter_shifted_offset(int16_t *, uint32x4_t, int32x4_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vstrhq_scatter_shifted_offset_u16)))
void vstrhq_scatter_shifted_offset_u16(uint16_t *, uint16x8_t, uint16x8_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vstrhq_scatter_shifted_offset_u16)))
void vstrhq_scatter_shifted_offset(uint16_t *, uint16x8_t, uint16x8_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vstrhq_scatter_shifted_offset_u32)))
void vstrhq_scatter_shifted_offset_u32(uint16_t *, uint32x4_t, uint32x4_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vstrhq_scatter_shifted_offset_u32)))
void vstrhq_scatter_shifted_offset(uint16_t *, uint32x4_t, uint32x4_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vstrhq_u16)))
void vstrhq_u16(uint16_t *, uint16x8_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vstrhq_u16)))
void vstrhq(uint16_t *, uint16x8_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vstrhq_u32)))
void vstrhq_u32(uint16_t *, uint32x4_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vstrhq_u32)))
void vstrhq(uint16_t *, uint32x4_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vstrwq_p_s32)))
void vstrwq_p_s32(int32_t *, int32x4_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vstrwq_p_s32)))
void vstrwq_p(int32_t *, int32x4_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vstrwq_p_u32)))
void vstrwq_p_u32(uint32_t *, uint32x4_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vstrwq_p_u32)))
void vstrwq_p(uint32_t *, uint32x4_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vstrwq_s32)))
void vstrwq_s32(int32_t *, int32x4_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vstrwq_s32)))
void vstrwq(int32_t *, int32x4_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vstrwq_scatter_base_p_s32)))
void vstrwq_scatter_base_p_s32(uint32x4_t, int, int32x4_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vstrwq_scatter_base_p_s32)))
void vstrwq_scatter_base_p(uint32x4_t, int, int32x4_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vstrwq_scatter_base_p_u32)))
void vstrwq_scatter_base_p_u32(uint32x4_t, int, uint32x4_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vstrwq_scatter_base_p_u32)))
void vstrwq_scatter_base_p(uint32x4_t, int, uint32x4_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vstrwq_scatter_base_s32)))
void vstrwq_scatter_base_s32(uint32x4_t, int, int32x4_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vstrwq_scatter_base_s32)))
void vstrwq_scatter_base(uint32x4_t, int, int32x4_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vstrwq_scatter_base_u32)))
void vstrwq_scatter_base_u32(uint32x4_t, int, uint32x4_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vstrwq_scatter_base_u32)))
void vstrwq_scatter_base(uint32x4_t, int, uint32x4_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vstrwq_scatter_base_wb_p_s32)))
void vstrwq_scatter_base_wb_p_s32(uint32x4_t *, int, int32x4_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vstrwq_scatter_base_wb_p_s32)))
void vstrwq_scatter_base_wb_p(uint32x4_t *, int, int32x4_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vstrwq_scatter_base_wb_p_u32)))
void vstrwq_scatter_base_wb_p_u32(uint32x4_t *, int, uint32x4_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vstrwq_scatter_base_wb_p_u32)))
void vstrwq_scatter_base_wb_p(uint32x4_t *, int, uint32x4_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vstrwq_scatter_base_wb_s32)))
void vstrwq_scatter_base_wb_s32(uint32x4_t *, int, int32x4_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vstrwq_scatter_base_wb_s32)))
void vstrwq_scatter_base_wb(uint32x4_t *, int, int32x4_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vstrwq_scatter_base_wb_u32)))
void vstrwq_scatter_base_wb_u32(uint32x4_t *, int, uint32x4_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vstrwq_scatter_base_wb_u32)))
void vstrwq_scatter_base_wb(uint32x4_t *, int, uint32x4_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vstrwq_scatter_offset_p_s32)))
void vstrwq_scatter_offset_p_s32(int32_t *, uint32x4_t, int32x4_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vstrwq_scatter_offset_p_s32)))
void vstrwq_scatter_offset_p(int32_t *, uint32x4_t, int32x4_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vstrwq_scatter_offset_p_u32)))
void vstrwq_scatter_offset_p_u32(uint32_t *, uint32x4_t, uint32x4_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vstrwq_scatter_offset_p_u32)))
void vstrwq_scatter_offset_p(uint32_t *, uint32x4_t, uint32x4_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vstrwq_scatter_offset_s32)))
void vstrwq_scatter_offset_s32(int32_t *, uint32x4_t, int32x4_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vstrwq_scatter_offset_s32)))
void vstrwq_scatter_offset(int32_t *, uint32x4_t, int32x4_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vstrwq_scatter_offset_u32)))
void vstrwq_scatter_offset_u32(uint32_t *, uint32x4_t, uint32x4_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vstrwq_scatter_offset_u32)))
void vstrwq_scatter_offset(uint32_t *, uint32x4_t, uint32x4_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vstrwq_scatter_shifted_offset_p_s32)))
void vstrwq_scatter_shifted_offset_p_s32(int32_t *, uint32x4_t, int32x4_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vstrwq_scatter_shifted_offset_p_s32)))
void vstrwq_scatter_shifted_offset_p(int32_t *, uint32x4_t, int32x4_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vstrwq_scatter_shifted_offset_p_u32)))
void vstrwq_scatter_shifted_offset_p_u32(uint32_t *, uint32x4_t, uint32x4_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vstrwq_scatter_shifted_offset_p_u32)))
void vstrwq_scatter_shifted_offset_p(uint32_t *, uint32x4_t, uint32x4_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vstrwq_scatter_shifted_offset_s32)))
void vstrwq_scatter_shifted_offset_s32(int32_t *, uint32x4_t, int32x4_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vstrwq_scatter_shifted_offset_s32)))
void vstrwq_scatter_shifted_offset(int32_t *, uint32x4_t, int32x4_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vstrwq_scatter_shifted_offset_u32)))
void vstrwq_scatter_shifted_offset_u32(uint32_t *, uint32x4_t, uint32x4_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vstrwq_scatter_shifted_offset_u32)))
void vstrwq_scatter_shifted_offset(uint32_t *, uint32x4_t, uint32x4_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vstrwq_u32)))
void vstrwq_u32(uint32_t *, uint32x4_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vstrwq_u32)))
void vstrwq(uint32_t *, uint32x4_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vsubq_m_s16)))
int16x8_t vsubq_m_s16(int16x8_t, int16x8_t, int16x8_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vsubq_m_s16)))
int16x8_t vsubq_m(int16x8_t, int16x8_t, int16x8_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vsubq_m_s32)))
int32x4_t vsubq_m_s32(int32x4_t, int32x4_t, int32x4_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vsubq_m_s32)))
int32x4_t vsubq_m(int32x4_t, int32x4_t, int32x4_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vsubq_m_s8)))
int8x16_t vsubq_m_s8(int8x16_t, int8x16_t, int8x16_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vsubq_m_s8)))
int8x16_t vsubq_m(int8x16_t, int8x16_t, int8x16_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vsubq_m_u16)))
uint16x8_t vsubq_m_u16(uint16x8_t, uint16x8_t, uint16x8_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vsubq_m_u16)))
uint16x8_t vsubq_m(uint16x8_t, uint16x8_t, uint16x8_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vsubq_m_u32)))
uint32x4_t vsubq_m_u32(uint32x4_t, uint32x4_t, uint32x4_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vsubq_m_u32)))
uint32x4_t vsubq_m(uint32x4_t, uint32x4_t, uint32x4_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vsubq_m_u8)))
uint8x16_t vsubq_m_u8(uint8x16_t, uint8x16_t, uint8x16_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vsubq_m_u8)))
uint8x16_t vsubq_m(uint8x16_t, uint8x16_t, uint8x16_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vsubq_s16)))
int16x8_t vsubq_s16(int16x8_t, int16x8_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vsubq_s16)))
int16x8_t vsubq(int16x8_t, int16x8_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vsubq_s32)))
int32x4_t vsubq_s32(int32x4_t, int32x4_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vsubq_s32)))
int32x4_t vsubq(int32x4_t, int32x4_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vsubq_s8)))
int8x16_t vsubq_s8(int8x16_t, int8x16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vsubq_s8)))
int8x16_t vsubq(int8x16_t, int8x16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vsubq_u16)))
uint16x8_t vsubq_u16(uint16x8_t, uint16x8_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vsubq_u16)))
uint16x8_t vsubq(uint16x8_t, uint16x8_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vsubq_u32)))
uint32x4_t vsubq_u32(uint32x4_t, uint32x4_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vsubq_u32)))
uint32x4_t vsubq(uint32x4_t, uint32x4_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vsubq_u8)))
uint8x16_t vsubq_u8(uint8x16_t, uint8x16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vsubq_u8)))
uint8x16_t vsubq(uint8x16_t, uint8x16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vuninitializedq_polymorphic_s16)))
int16x8_t vuninitializedq(int16x8_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vuninitializedq_polymorphic_s32)))
int32x4_t vuninitializedq(int32x4_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vuninitializedq_polymorphic_s64)))
int64x2_t vuninitializedq(int64x2_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vuninitializedq_polymorphic_s8)))
int8x16_t vuninitializedq(int8x16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vuninitializedq_polymorphic_u16)))
uint16x8_t vuninitializedq(uint16x8_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vuninitializedq_polymorphic_u32)))
uint32x4_t vuninitializedq(uint32x4_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vuninitializedq_polymorphic_u64)))
uint64x2_t vuninitializedq(uint64x2_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vuninitializedq_polymorphic_u8)))
uint8x16_t vuninitializedq(uint8x16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vuninitializedq_s16)))
int16x8_t vuninitializedq_s16();
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vuninitializedq_s32)))
int32x4_t vuninitializedq_s32();
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vuninitializedq_s64)))
int64x2_t vuninitializedq_s64();
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vuninitializedq_s8)))
int8x16_t vuninitializedq_s8();
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vuninitializedq_u16)))
uint16x8_t vuninitializedq_u16();
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vuninitializedq_u32)))
uint32x4_t vuninitializedq_u32();
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vuninitializedq_u64)))
uint64x2_t vuninitializedq_u64();
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vuninitializedq_u8)))
uint8x16_t vuninitializedq_u8();

#endif /* (!defined __ARM_MVE_PRESERVE_USER_NAMESPACE) */

#if (__ARM_FEATURE_MVE & 2) && (!defined __ARM_MVE_PRESERVE_USER_NAMESPACE)

static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vabdq_f16)))
float16x8_t vabdq_f16(float16x8_t, float16x8_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vabdq_f16)))
float16x8_t vabdq(float16x8_t, float16x8_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vabdq_f32)))
float32x4_t vabdq_f32(float32x4_t, float32x4_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vabdq_f32)))
float32x4_t vabdq(float32x4_t, float32x4_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vabdq_m_f16)))
float16x8_t vabdq_m_f16(float16x8_t, float16x8_t, float16x8_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vabdq_m_f16)))
float16x8_t vabdq_m(float16x8_t, float16x8_t, float16x8_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vabdq_m_f32)))
float32x4_t vabdq_m_f32(float32x4_t, float32x4_t, float32x4_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vabdq_m_f32)))
float32x4_t vabdq_m(float32x4_t, float32x4_t, float32x4_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vaddq_f16)))
float16x8_t vaddq_f16(float16x8_t, float16x8_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vaddq_f16)))
float16x8_t vaddq(float16x8_t, float16x8_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vaddq_f32)))
float32x4_t vaddq_f32(float32x4_t, float32x4_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vaddq_f32)))
float32x4_t vaddq(float32x4_t, float32x4_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vaddq_m_f16)))
float16x8_t vaddq_m_f16(float16x8_t, float16x8_t, float16x8_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vaddq_m_f16)))
float16x8_t vaddq_m(float16x8_t, float16x8_t, float16x8_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vaddq_m_f32)))
float32x4_t vaddq_m_f32(float32x4_t, float32x4_t, float32x4_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vaddq_m_f32)))
float32x4_t vaddq_m(float32x4_t, float32x4_t, float32x4_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vandq_f16)))
float16x8_t vandq_f16(float16x8_t, float16x8_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vandq_f16)))
float16x8_t vandq(float16x8_t, float16x8_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vandq_f32)))
float32x4_t vandq_f32(float32x4_t, float32x4_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vandq_f32)))
float32x4_t vandq(float32x4_t, float32x4_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vandq_m_f16)))
float16x8_t vandq_m_f16(float16x8_t, float16x8_t, float16x8_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vandq_m_f16)))
float16x8_t vandq_m(float16x8_t, float16x8_t, float16x8_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vandq_m_f32)))
float32x4_t vandq_m_f32(float32x4_t, float32x4_t, float32x4_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vandq_m_f32)))
float32x4_t vandq_m(float32x4_t, float32x4_t, float32x4_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vbicq_f16)))
float16x8_t vbicq_f16(float16x8_t, float16x8_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vbicq_f16)))
float16x8_t vbicq(float16x8_t, float16x8_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vbicq_f32)))
float32x4_t vbicq_f32(float32x4_t, float32x4_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vbicq_f32)))
float32x4_t vbicq(float32x4_t, float32x4_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vbicq_m_f16)))
float16x8_t vbicq_m_f16(float16x8_t, float16x8_t, float16x8_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vbicq_m_f16)))
float16x8_t vbicq_m(float16x8_t, float16x8_t, float16x8_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vbicq_m_f32)))
float32x4_t vbicq_m_f32(float32x4_t, float32x4_t, float32x4_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vbicq_m_f32)))
float32x4_t vbicq_m(float32x4_t, float32x4_t, float32x4_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpeqq_f16)))
mve_pred16_t vcmpeqq_f16(float16x8_t, float16x8_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpeqq_f16)))
mve_pred16_t vcmpeqq(float16x8_t, float16x8_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpeqq_f32)))
mve_pred16_t vcmpeqq_f32(float32x4_t, float32x4_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpeqq_f32)))
mve_pred16_t vcmpeqq(float32x4_t, float32x4_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpeqq_m_f16)))
mve_pred16_t vcmpeqq_m_f16(float16x8_t, float16x8_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpeqq_m_f16)))
mve_pred16_t vcmpeqq_m(float16x8_t, float16x8_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpeqq_m_f32)))
mve_pred16_t vcmpeqq_m_f32(float32x4_t, float32x4_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpeqq_m_f32)))
mve_pred16_t vcmpeqq_m(float32x4_t, float32x4_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpeqq_m_n_f16)))
mve_pred16_t vcmpeqq_m_n_f16(float16x8_t, float16_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpeqq_m_n_f16)))
mve_pred16_t vcmpeqq_m(float16x8_t, float16_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpeqq_m_n_f32)))
mve_pred16_t vcmpeqq_m_n_f32(float32x4_t, float32_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpeqq_m_n_f32)))
mve_pred16_t vcmpeqq_m(float32x4_t, float32_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpeqq_n_f16)))
mve_pred16_t vcmpeqq_n_f16(float16x8_t, float16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpeqq_n_f16)))
mve_pred16_t vcmpeqq(float16x8_t, float16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpeqq_n_f32)))
mve_pred16_t vcmpeqq_n_f32(float32x4_t, float32_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpeqq_n_f32)))
mve_pred16_t vcmpeqq(float32x4_t, float32_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpgeq_f16)))
mve_pred16_t vcmpgeq_f16(float16x8_t, float16x8_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpgeq_f16)))
mve_pred16_t vcmpgeq(float16x8_t, float16x8_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpgeq_f32)))
mve_pred16_t vcmpgeq_f32(float32x4_t, float32x4_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpgeq_f32)))
mve_pred16_t vcmpgeq(float32x4_t, float32x4_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpgeq_m_f16)))
mve_pred16_t vcmpgeq_m_f16(float16x8_t, float16x8_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpgeq_m_f16)))
mve_pred16_t vcmpgeq_m(float16x8_t, float16x8_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpgeq_m_f32)))
mve_pred16_t vcmpgeq_m_f32(float32x4_t, float32x4_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpgeq_m_f32)))
mve_pred16_t vcmpgeq_m(float32x4_t, float32x4_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpgeq_m_n_f16)))
mve_pred16_t vcmpgeq_m_n_f16(float16x8_t, float16_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpgeq_m_n_f16)))
mve_pred16_t vcmpgeq_m(float16x8_t, float16_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpgeq_m_n_f32)))
mve_pred16_t vcmpgeq_m_n_f32(float32x4_t, float32_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpgeq_m_n_f32)))
mve_pred16_t vcmpgeq_m(float32x4_t, float32_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpgeq_n_f16)))
mve_pred16_t vcmpgeq_n_f16(float16x8_t, float16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpgeq_n_f16)))
mve_pred16_t vcmpgeq(float16x8_t, float16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpgeq_n_f32)))
mve_pred16_t vcmpgeq_n_f32(float32x4_t, float32_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpgeq_n_f32)))
mve_pred16_t vcmpgeq(float32x4_t, float32_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpgtq_f16)))
mve_pred16_t vcmpgtq_f16(float16x8_t, float16x8_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpgtq_f16)))
mve_pred16_t vcmpgtq(float16x8_t, float16x8_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpgtq_f32)))
mve_pred16_t vcmpgtq_f32(float32x4_t, float32x4_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpgtq_f32)))
mve_pred16_t vcmpgtq(float32x4_t, float32x4_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpgtq_m_f16)))
mve_pred16_t vcmpgtq_m_f16(float16x8_t, float16x8_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpgtq_m_f16)))
mve_pred16_t vcmpgtq_m(float16x8_t, float16x8_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpgtq_m_f32)))
mve_pred16_t vcmpgtq_m_f32(float32x4_t, float32x4_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpgtq_m_f32)))
mve_pred16_t vcmpgtq_m(float32x4_t, float32x4_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpgtq_m_n_f16)))
mve_pred16_t vcmpgtq_m_n_f16(float16x8_t, float16_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpgtq_m_n_f16)))
mve_pred16_t vcmpgtq_m(float16x8_t, float16_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpgtq_m_n_f32)))
mve_pred16_t vcmpgtq_m_n_f32(float32x4_t, float32_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpgtq_m_n_f32)))
mve_pred16_t vcmpgtq_m(float32x4_t, float32_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpgtq_n_f16)))
mve_pred16_t vcmpgtq_n_f16(float16x8_t, float16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpgtq_n_f16)))
mve_pred16_t vcmpgtq(float16x8_t, float16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpgtq_n_f32)))
mve_pred16_t vcmpgtq_n_f32(float32x4_t, float32_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpgtq_n_f32)))
mve_pred16_t vcmpgtq(float32x4_t, float32_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpleq_f16)))
mve_pred16_t vcmpleq_f16(float16x8_t, float16x8_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpleq_f16)))
mve_pred16_t vcmpleq(float16x8_t, float16x8_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpleq_f32)))
mve_pred16_t vcmpleq_f32(float32x4_t, float32x4_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpleq_f32)))
mve_pred16_t vcmpleq(float32x4_t, float32x4_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpleq_m_f16)))
mve_pred16_t vcmpleq_m_f16(float16x8_t, float16x8_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpleq_m_f16)))
mve_pred16_t vcmpleq_m(float16x8_t, float16x8_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpleq_m_f32)))
mve_pred16_t vcmpleq_m_f32(float32x4_t, float32x4_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpleq_m_f32)))
mve_pred16_t vcmpleq_m(float32x4_t, float32x4_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpleq_m_n_f16)))
mve_pred16_t vcmpleq_m_n_f16(float16x8_t, float16_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpleq_m_n_f16)))
mve_pred16_t vcmpleq_m(float16x8_t, float16_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpleq_m_n_f32)))
mve_pred16_t vcmpleq_m_n_f32(float32x4_t, float32_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpleq_m_n_f32)))
mve_pred16_t vcmpleq_m(float32x4_t, float32_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpleq_n_f16)))
mve_pred16_t vcmpleq_n_f16(float16x8_t, float16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpleq_n_f16)))
mve_pred16_t vcmpleq(float16x8_t, float16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpleq_n_f32)))
mve_pred16_t vcmpleq_n_f32(float32x4_t, float32_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpleq_n_f32)))
mve_pred16_t vcmpleq(float32x4_t, float32_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpltq_f16)))
mve_pred16_t vcmpltq_f16(float16x8_t, float16x8_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpltq_f16)))
mve_pred16_t vcmpltq(float16x8_t, float16x8_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpltq_f32)))
mve_pred16_t vcmpltq_f32(float32x4_t, float32x4_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpltq_f32)))
mve_pred16_t vcmpltq(float32x4_t, float32x4_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpltq_m_f16)))
mve_pred16_t vcmpltq_m_f16(float16x8_t, float16x8_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpltq_m_f16)))
mve_pred16_t vcmpltq_m(float16x8_t, float16x8_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpltq_m_f32)))
mve_pred16_t vcmpltq_m_f32(float32x4_t, float32x4_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpltq_m_f32)))
mve_pred16_t vcmpltq_m(float32x4_t, float32x4_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpltq_m_n_f16)))
mve_pred16_t vcmpltq_m_n_f16(float16x8_t, float16_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpltq_m_n_f16)))
mve_pred16_t vcmpltq_m(float16x8_t, float16_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpltq_m_n_f32)))
mve_pred16_t vcmpltq_m_n_f32(float32x4_t, float32_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpltq_m_n_f32)))
mve_pred16_t vcmpltq_m(float32x4_t, float32_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpltq_n_f16)))
mve_pred16_t vcmpltq_n_f16(float16x8_t, float16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpltq_n_f16)))
mve_pred16_t vcmpltq(float16x8_t, float16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpltq_n_f32)))
mve_pred16_t vcmpltq_n_f32(float32x4_t, float32_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpltq_n_f32)))
mve_pred16_t vcmpltq(float32x4_t, float32_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpneq_f16)))
mve_pred16_t vcmpneq_f16(float16x8_t, float16x8_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpneq_f16)))
mve_pred16_t vcmpneq(float16x8_t, float16x8_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpneq_f32)))
mve_pred16_t vcmpneq_f32(float32x4_t, float32x4_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpneq_f32)))
mve_pred16_t vcmpneq(float32x4_t, float32x4_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpneq_m_f16)))
mve_pred16_t vcmpneq_m_f16(float16x8_t, float16x8_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpneq_m_f16)))
mve_pred16_t vcmpneq_m(float16x8_t, float16x8_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpneq_m_f32)))
mve_pred16_t vcmpneq_m_f32(float32x4_t, float32x4_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpneq_m_f32)))
mve_pred16_t vcmpneq_m(float32x4_t, float32x4_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpneq_m_n_f16)))
mve_pred16_t vcmpneq_m_n_f16(float16x8_t, float16_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpneq_m_n_f16)))
mve_pred16_t vcmpneq_m(float16x8_t, float16_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpneq_m_n_f32)))
mve_pred16_t vcmpneq_m_n_f32(float32x4_t, float32_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpneq_m_n_f32)))
mve_pred16_t vcmpneq_m(float32x4_t, float32_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpneq_n_f16)))
mve_pred16_t vcmpneq_n_f16(float16x8_t, float16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpneq_n_f16)))
mve_pred16_t vcmpneq(float16x8_t, float16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcmpneq_n_f32)))
mve_pred16_t vcmpneq_n_f32(float32x4_t, float32_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vcmpneq_n_f32)))
mve_pred16_t vcmpneq(float32x4_t, float32_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcreateq_f16)))
float16x8_t vcreateq_f16(uint64_t, uint64_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcreateq_f32)))
float32x4_t vcreateq_f32(uint64_t, uint64_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcvtbq_f16_f32)))
float16x8_t vcvtbq_f16_f32(float16x8_t, float32x4_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcvtbq_m_f16_f32)))
float16x8_t vcvtbq_m_f16_f32(float16x8_t, float32x4_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcvttq_f16_f32)))
float16x8_t vcvttq_f16_f32(float16x8_t, float32x4_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vcvttq_m_f16_f32)))
float16x8_t vcvttq_m_f16_f32(float16x8_t, float32x4_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_veorq_f16)))
float16x8_t veorq_f16(float16x8_t, float16x8_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_veorq_f16)))
float16x8_t veorq(float16x8_t, float16x8_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_veorq_f32)))
float32x4_t veorq_f32(float32x4_t, float32x4_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_veorq_f32)))
float32x4_t veorq(float32x4_t, float32x4_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_veorq_m_f16)))
float16x8_t veorq_m_f16(float16x8_t, float16x8_t, float16x8_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_veorq_m_f16)))
float16x8_t veorq_m(float16x8_t, float16x8_t, float16x8_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_veorq_m_f32)))
float32x4_t veorq_m_f32(float32x4_t, float32x4_t, float32x4_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_veorq_m_f32)))
float32x4_t veorq_m(float32x4_t, float32x4_t, float32x4_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vgetq_lane_f16)))
float16_t vgetq_lane_f16(float16x8_t, int);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vgetq_lane_f16)))
float16_t vgetq_lane(float16x8_t, int);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vgetq_lane_f32)))
float32_t vgetq_lane_f32(float32x4_t, int);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vgetq_lane_f32)))
float32_t vgetq_lane(float32x4_t, int);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vld1q_f16)))
float16x8_t vld1q_f16(const float16_t *);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vld1q_f16)))
float16x8_t vld1q(const float16_t *);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vld1q_f32)))
float32x4_t vld1q_f32(const float32_t *);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vld1q_f32)))
float32x4_t vld1q(const float32_t *);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vld1q_z_f16)))
float16x8_t vld1q_z_f16(const float16_t *, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vld1q_z_f16)))
float16x8_t vld1q_z(const float16_t *, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vld1q_z_f32)))
float32x4_t vld1q_z_f32(const float32_t *, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vld1q_z_f32)))
float32x4_t vld1q_z(const float32_t *, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vld2q_f16)))
float16x8x2_t vld2q_f16(const float16_t *);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vld2q_f16)))
float16x8x2_t vld2q(const float16_t *);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vld2q_f32)))
float32x4x2_t vld2q_f32(const float32_t *);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vld2q_f32)))
float32x4x2_t vld2q(const float32_t *);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vld4q_f16)))
float16x8x4_t vld4q_f16(const float16_t *);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vld4q_f16)))
float16x8x4_t vld4q(const float16_t *);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vld4q_f32)))
float32x4x4_t vld4q_f32(const float32_t *);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vld4q_f32)))
float32x4x4_t vld4q(const float32_t *);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vldrhq_f16)))
float16x8_t vldrhq_f16(const float16_t *);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vldrhq_gather_offset_f16)))
float16x8_t vldrhq_gather_offset_f16(const float16_t *, uint16x8_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vldrhq_gather_offset_f16)))
float16x8_t vldrhq_gather_offset(const float16_t *, uint16x8_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vldrhq_gather_offset_z_f16)))
float16x8_t vldrhq_gather_offset_z_f16(const float16_t *, uint16x8_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vldrhq_gather_offset_z_f16)))
float16x8_t vldrhq_gather_offset_z(const float16_t *, uint16x8_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vldrhq_gather_shifted_offset_f16)))
float16x8_t vldrhq_gather_shifted_offset_f16(const float16_t *, uint16x8_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vldrhq_gather_shifted_offset_f16)))
float16x8_t vldrhq_gather_shifted_offset(const float16_t *, uint16x8_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vldrhq_gather_shifted_offset_z_f16)))
float16x8_t vldrhq_gather_shifted_offset_z_f16(const float16_t *, uint16x8_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vldrhq_gather_shifted_offset_z_f16)))
float16x8_t vldrhq_gather_shifted_offset_z(const float16_t *, uint16x8_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vldrhq_z_f16)))
float16x8_t vldrhq_z_f16(const float16_t *, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vldrwq_f32)))
float32x4_t vldrwq_f32(const float32_t *);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vldrwq_gather_base_f32)))
float32x4_t vldrwq_gather_base_f32(uint32x4_t, int);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vldrwq_gather_base_wb_f32)))
float32x4_t vldrwq_gather_base_wb_f32(uint32x4_t *, int);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vldrwq_gather_base_wb_z_f32)))
float32x4_t vldrwq_gather_base_wb_z_f32(uint32x4_t *, int, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vldrwq_gather_base_z_f32)))
float32x4_t vldrwq_gather_base_z_f32(uint32x4_t, int, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vldrwq_gather_offset_f32)))
float32x4_t vldrwq_gather_offset_f32(const float32_t *, uint32x4_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vldrwq_gather_offset_f32)))
float32x4_t vldrwq_gather_offset(const float32_t *, uint32x4_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vldrwq_gather_offset_z_f32)))
float32x4_t vldrwq_gather_offset_z_f32(const float32_t *, uint32x4_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vldrwq_gather_offset_z_f32)))
float32x4_t vldrwq_gather_offset_z(const float32_t *, uint32x4_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vldrwq_gather_shifted_offset_f32)))
float32x4_t vldrwq_gather_shifted_offset_f32(const float32_t *, uint32x4_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vldrwq_gather_shifted_offset_f32)))
float32x4_t vldrwq_gather_shifted_offset(const float32_t *, uint32x4_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vldrwq_gather_shifted_offset_z_f32)))
float32x4_t vldrwq_gather_shifted_offset_z_f32(const float32_t *, uint32x4_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vldrwq_gather_shifted_offset_z_f32)))
float32x4_t vldrwq_gather_shifted_offset_z(const float32_t *, uint32x4_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vldrwq_z_f32)))
float32x4_t vldrwq_z_f32(const float32_t *, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vmulq_f16)))
float16x8_t vmulq_f16(float16x8_t, float16x8_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vmulq_f16)))
float16x8_t vmulq(float16x8_t, float16x8_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vmulq_f32)))
float32x4_t vmulq_f32(float32x4_t, float32x4_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vmulq_f32)))
float32x4_t vmulq(float32x4_t, float32x4_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vmulq_m_f16)))
float16x8_t vmulq_m_f16(float16x8_t, float16x8_t, float16x8_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vmulq_m_f16)))
float16x8_t vmulq_m(float16x8_t, float16x8_t, float16x8_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vmulq_m_f32)))
float32x4_t vmulq_m_f32(float32x4_t, float32x4_t, float32x4_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vmulq_m_f32)))
float32x4_t vmulq_m(float32x4_t, float32x4_t, float32x4_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vornq_f16)))
float16x8_t vornq_f16(float16x8_t, float16x8_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vornq_f16)))
float16x8_t vornq(float16x8_t, float16x8_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vornq_f32)))
float32x4_t vornq_f32(float32x4_t, float32x4_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vornq_f32)))
float32x4_t vornq(float32x4_t, float32x4_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vornq_m_f16)))
float16x8_t vornq_m_f16(float16x8_t, float16x8_t, float16x8_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vornq_m_f16)))
float16x8_t vornq_m(float16x8_t, float16x8_t, float16x8_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vornq_m_f32)))
float32x4_t vornq_m_f32(float32x4_t, float32x4_t, float32x4_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vornq_m_f32)))
float32x4_t vornq_m(float32x4_t, float32x4_t, float32x4_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vorrq_f16)))
float16x8_t vorrq_f16(float16x8_t, float16x8_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vorrq_f16)))
float16x8_t vorrq(float16x8_t, float16x8_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vorrq_f32)))
float32x4_t vorrq_f32(float32x4_t, float32x4_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vorrq_f32)))
float32x4_t vorrq(float32x4_t, float32x4_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vorrq_m_f16)))
float16x8_t vorrq_m_f16(float16x8_t, float16x8_t, float16x8_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vorrq_m_f16)))
float16x8_t vorrq_m(float16x8_t, float16x8_t, float16x8_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vorrq_m_f32)))
float32x4_t vorrq_m_f32(float32x4_t, float32x4_t, float32x4_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vorrq_m_f32)))
float32x4_t vorrq_m(float32x4_t, float32x4_t, float32x4_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_f16_f32)))
float16x8_t vreinterpretq_f16_f32(float32x4_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_f16_f32)))
float16x8_t vreinterpretq_f16(float32x4_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_f16_s16)))
float16x8_t vreinterpretq_f16_s16(int16x8_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_f16_s16)))
float16x8_t vreinterpretq_f16(int16x8_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_f16_s32)))
float16x8_t vreinterpretq_f16_s32(int32x4_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_f16_s32)))
float16x8_t vreinterpretq_f16(int32x4_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_f16_s64)))
float16x8_t vreinterpretq_f16_s64(int64x2_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_f16_s64)))
float16x8_t vreinterpretq_f16(int64x2_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_f16_s8)))
float16x8_t vreinterpretq_f16_s8(int8x16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_f16_s8)))
float16x8_t vreinterpretq_f16(int8x16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_f16_u16)))
float16x8_t vreinterpretq_f16_u16(uint16x8_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_f16_u16)))
float16x8_t vreinterpretq_f16(uint16x8_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_f16_u32)))
float16x8_t vreinterpretq_f16_u32(uint32x4_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_f16_u32)))
float16x8_t vreinterpretq_f16(uint32x4_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_f16_u64)))
float16x8_t vreinterpretq_f16_u64(uint64x2_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_f16_u64)))
float16x8_t vreinterpretq_f16(uint64x2_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_f16_u8)))
float16x8_t vreinterpretq_f16_u8(uint8x16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_f16_u8)))
float16x8_t vreinterpretq_f16(uint8x16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_f32_f16)))
float32x4_t vreinterpretq_f32_f16(float16x8_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_f32_f16)))
float32x4_t vreinterpretq_f32(float16x8_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_f32_s16)))
float32x4_t vreinterpretq_f32_s16(int16x8_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_f32_s16)))
float32x4_t vreinterpretq_f32(int16x8_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_f32_s32)))
float32x4_t vreinterpretq_f32_s32(int32x4_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_f32_s32)))
float32x4_t vreinterpretq_f32(int32x4_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_f32_s64)))
float32x4_t vreinterpretq_f32_s64(int64x2_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_f32_s64)))
float32x4_t vreinterpretq_f32(int64x2_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_f32_s8)))
float32x4_t vreinterpretq_f32_s8(int8x16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_f32_s8)))
float32x4_t vreinterpretq_f32(int8x16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_f32_u16)))
float32x4_t vreinterpretq_f32_u16(uint16x8_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_f32_u16)))
float32x4_t vreinterpretq_f32(uint16x8_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_f32_u32)))
float32x4_t vreinterpretq_f32_u32(uint32x4_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_f32_u32)))
float32x4_t vreinterpretq_f32(uint32x4_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_f32_u64)))
float32x4_t vreinterpretq_f32_u64(uint64x2_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_f32_u64)))
float32x4_t vreinterpretq_f32(uint64x2_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_f32_u8)))
float32x4_t vreinterpretq_f32_u8(uint8x16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_f32_u8)))
float32x4_t vreinterpretq_f32(uint8x16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_s16_f16)))
int16x8_t vreinterpretq_s16_f16(float16x8_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_s16_f16)))
int16x8_t vreinterpretq_s16(float16x8_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_s16_f32)))
int16x8_t vreinterpretq_s16_f32(float32x4_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_s16_f32)))
int16x8_t vreinterpretq_s16(float32x4_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_s32_f16)))
int32x4_t vreinterpretq_s32_f16(float16x8_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_s32_f16)))
int32x4_t vreinterpretq_s32(float16x8_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_s32_f32)))
int32x4_t vreinterpretq_s32_f32(float32x4_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_s32_f32)))
int32x4_t vreinterpretq_s32(float32x4_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_s64_f16)))
int64x2_t vreinterpretq_s64_f16(float16x8_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_s64_f16)))
int64x2_t vreinterpretq_s64(float16x8_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_s64_f32)))
int64x2_t vreinterpretq_s64_f32(float32x4_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_s64_f32)))
int64x2_t vreinterpretq_s64(float32x4_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_s8_f16)))
int8x16_t vreinterpretq_s8_f16(float16x8_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_s8_f16)))
int8x16_t vreinterpretq_s8(float16x8_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_s8_f32)))
int8x16_t vreinterpretq_s8_f32(float32x4_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_s8_f32)))
int8x16_t vreinterpretq_s8(float32x4_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_u16_f16)))
uint16x8_t vreinterpretq_u16_f16(float16x8_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_u16_f16)))
uint16x8_t vreinterpretq_u16(float16x8_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_u16_f32)))
uint16x8_t vreinterpretq_u16_f32(float32x4_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_u16_f32)))
uint16x8_t vreinterpretq_u16(float32x4_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_u32_f16)))
uint32x4_t vreinterpretq_u32_f16(float16x8_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_u32_f16)))
uint32x4_t vreinterpretq_u32(float16x8_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_u32_f32)))
uint32x4_t vreinterpretq_u32_f32(float32x4_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_u32_f32)))
uint32x4_t vreinterpretq_u32(float32x4_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_u64_f16)))
uint64x2_t vreinterpretq_u64_f16(float16x8_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_u64_f16)))
uint64x2_t vreinterpretq_u64(float16x8_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_u64_f32)))
uint64x2_t vreinterpretq_u64_f32(float32x4_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_u64_f32)))
uint64x2_t vreinterpretq_u64(float32x4_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_u8_f16)))
uint8x16_t vreinterpretq_u8_f16(float16x8_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_u8_f16)))
uint8x16_t vreinterpretq_u8(float16x8_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_u8_f32)))
uint8x16_t vreinterpretq_u8_f32(float32x4_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vreinterpretq_u8_f32)))
uint8x16_t vreinterpretq_u8(float32x4_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vsetq_lane_f16)))
float16x8_t vsetq_lane_f16(float16_t, float16x8_t, int);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vsetq_lane_f16)))
float16x8_t vsetq_lane(float16_t, float16x8_t, int);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vsetq_lane_f32)))
float32x4_t vsetq_lane_f32(float32_t, float32x4_t, int);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vsetq_lane_f32)))
float32x4_t vsetq_lane(float32_t, float32x4_t, int);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vst1q_f16)))
void vst1q_f16(float16_t *, float16x8_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vst1q_f16)))
void vst1q(float16_t *, float16x8_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vst1q_f32)))
void vst1q_f32(float32_t *, float32x4_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vst1q_f32)))
void vst1q(float32_t *, float32x4_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vst1q_p_f16)))
void vst1q_p_f16(float16_t *, float16x8_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vst1q_p_f16)))
void vst1q_p(float16_t *, float16x8_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vst1q_p_f32)))
void vst1q_p_f32(float32_t *, float32x4_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vst1q_p_f32)))
void vst1q_p(float32_t *, float32x4_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vst2q_f16)))
void vst2q_f16(float16_t *, float16x8x2_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vst2q_f16)))
void vst2q(float16_t *, float16x8x2_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vst2q_f32)))
void vst2q_f32(float32_t *, float32x4x2_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vst2q_f32)))
void vst2q(float32_t *, float32x4x2_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vst4q_f16)))
void vst4q_f16(float16_t *, float16x8x4_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vst4q_f16)))
void vst4q(float16_t *, float16x8x4_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vst4q_f32)))
void vst4q_f32(float32_t *, float32x4x4_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vst4q_f32)))
void vst4q(float32_t *, float32x4x4_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vstrhq_f16)))
void vstrhq_f16(float16_t *, float16x8_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vstrhq_f16)))
void vstrhq(float16_t *, float16x8_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vstrhq_p_f16)))
void vstrhq_p_f16(float16_t *, float16x8_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vstrhq_p_f16)))
void vstrhq_p(float16_t *, float16x8_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vstrhq_scatter_offset_f16)))
void vstrhq_scatter_offset_f16(float16_t *, uint16x8_t, float16x8_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vstrhq_scatter_offset_f16)))
void vstrhq_scatter_offset(float16_t *, uint16x8_t, float16x8_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vstrhq_scatter_offset_p_f16)))
void vstrhq_scatter_offset_p_f16(float16_t *, uint16x8_t, float16x8_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vstrhq_scatter_offset_p_f16)))
void vstrhq_scatter_offset_p(float16_t *, uint16x8_t, float16x8_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vstrhq_scatter_shifted_offset_f16)))
void vstrhq_scatter_shifted_offset_f16(float16_t *, uint16x8_t, float16x8_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vstrhq_scatter_shifted_offset_f16)))
void vstrhq_scatter_shifted_offset(float16_t *, uint16x8_t, float16x8_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vstrhq_scatter_shifted_offset_p_f16)))
void vstrhq_scatter_shifted_offset_p_f16(float16_t *, uint16x8_t, float16x8_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vstrhq_scatter_shifted_offset_p_f16)))
void vstrhq_scatter_shifted_offset_p(float16_t *, uint16x8_t, float16x8_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vstrwq_f32)))
void vstrwq_f32(float32_t *, float32x4_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vstrwq_f32)))
void vstrwq(float32_t *, float32x4_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vstrwq_p_f32)))
void vstrwq_p_f32(float32_t *, float32x4_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vstrwq_p_f32)))
void vstrwq_p(float32_t *, float32x4_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vstrwq_scatter_base_f32)))
void vstrwq_scatter_base_f32(uint32x4_t, int, float32x4_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vstrwq_scatter_base_f32)))
void vstrwq_scatter_base(uint32x4_t, int, float32x4_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vstrwq_scatter_base_p_f32)))
void vstrwq_scatter_base_p_f32(uint32x4_t, int, float32x4_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vstrwq_scatter_base_p_f32)))
void vstrwq_scatter_base_p(uint32x4_t, int, float32x4_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vstrwq_scatter_base_wb_f32)))
void vstrwq_scatter_base_wb_f32(uint32x4_t *, int, float32x4_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vstrwq_scatter_base_wb_f32)))
void vstrwq_scatter_base_wb(uint32x4_t *, int, float32x4_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vstrwq_scatter_base_wb_p_f32)))
void vstrwq_scatter_base_wb_p_f32(uint32x4_t *, int, float32x4_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vstrwq_scatter_base_wb_p_f32)))
void vstrwq_scatter_base_wb_p(uint32x4_t *, int, float32x4_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vstrwq_scatter_offset_f32)))
void vstrwq_scatter_offset_f32(float32_t *, uint32x4_t, float32x4_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vstrwq_scatter_offset_f32)))
void vstrwq_scatter_offset(float32_t *, uint32x4_t, float32x4_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vstrwq_scatter_offset_p_f32)))
void vstrwq_scatter_offset_p_f32(float32_t *, uint32x4_t, float32x4_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vstrwq_scatter_offset_p_f32)))
void vstrwq_scatter_offset_p(float32_t *, uint32x4_t, float32x4_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vstrwq_scatter_shifted_offset_f32)))
void vstrwq_scatter_shifted_offset_f32(float32_t *, uint32x4_t, float32x4_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vstrwq_scatter_shifted_offset_f32)))
void vstrwq_scatter_shifted_offset(float32_t *, uint32x4_t, float32x4_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vstrwq_scatter_shifted_offset_p_f32)))
void vstrwq_scatter_shifted_offset_p_f32(float32_t *, uint32x4_t, float32x4_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vstrwq_scatter_shifted_offset_p_f32)))
void vstrwq_scatter_shifted_offset_p(float32_t *, uint32x4_t, float32x4_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vsubq_f16)))
float16x8_t vsubq_f16(float16x8_t, float16x8_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vsubq_f16)))
float16x8_t vsubq(float16x8_t, float16x8_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vsubq_f32)))
float32x4_t vsubq_f32(float32x4_t, float32x4_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vsubq_f32)))
float32x4_t vsubq(float32x4_t, float32x4_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vsubq_m_f16)))
float16x8_t vsubq_m_f16(float16x8_t, float16x8_t, float16x8_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vsubq_m_f16)))
float16x8_t vsubq_m(float16x8_t, float16x8_t, float16x8_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vsubq_m_f32)))
float32x4_t vsubq_m_f32(float32x4_t, float32x4_t, float32x4_t, mve_pred16_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vsubq_m_f32)))
float32x4_t vsubq_m(float32x4_t, float32x4_t, float32x4_t, mve_pred16_t);
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vuninitializedq_f16)))
float16x8_t vuninitializedq_f16();
static __inline__ __attribute__((__clang_arm_mve_alias(__builtin_arm_mve_vuninitializedq_f32)))
float32x4_t vuninitializedq_f32();
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vuninitializedq_polymorphic_f16)))
float16x8_t vuninitializedq(float16x8_t);
static __inline__ __attribute__((overloadable, __clang_arm_mve_alias(__builtin_arm_mve_vuninitializedq_polymorphic_f32)))
float32x4_t vuninitializedq(float32x4_t);

#endif /* (__ARM_FEATURE_MVE & 2) && (!defined __ARM_MVE_PRESERVE_USER_NAMESPACE) */

#endif /* __ARM_MVE_H */

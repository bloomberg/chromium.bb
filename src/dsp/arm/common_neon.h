/*
 * Copyright 2019 The libgav1 Authors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef LIBGAV1_SRC_DSP_ARM_COMMON_NEON_H_
#define LIBGAV1_SRC_DSP_ARM_COMMON_NEON_H_

#include "src/utils/cpu.h"

#if LIBGAV1_ENABLE_NEON

#include <arm_neon.h>

#include <cstdint>
#include <cstring>

#if 0
#include <cstdio>

#include "third_party/abseil-cpp/absl/strings/str_cat.h"

constexpr bool kEnablePrintRegs = true;

union DebugRegister {
  int8_t i8[8];
  int16_t i16[4];
  int32_t i32[2];
  uint8_t u8[8];
  uint16_t u16[4];
  uint32_t u32[2];
};

union DebugRegisterQ {
  int8_t i8[16];
  int16_t i16[8];
  int32_t i32[4];
  uint8_t u8[16];
  uint16_t u16[8];
  uint32_t u32[4];
};

// Quite useful macro for debugging. Left here for convenience.
inline void PrintVect(const DebugRegister r, const char* const name, int size) {
  int n;
  if (kEnablePrintRegs) {
    fprintf(stderr, "%s\t: ", name);
    if (size == 8) {
      for (n = 0; n < 8; ++n) fprintf(stderr, "%.2x ", r.u8[n]);
    } else if (size == 16) {
      for (n = 0; n < 4; ++n) fprintf(stderr, "%.4x ", r.u16[n]);
    } else if (size == 32) {
      for (n = 0; n < 2; ++n) fprintf(stderr, "%.8x ", r.u32[n]);
    }
    fprintf(stderr, "\n");
  }
}

// Debugging macro for 128-bit types.
inline void PrintVectQ(const DebugRegisterQ r, const char* const name,
                       int size) {
  int n;
  if (kEnablePrintRegs) {
    fprintf(stderr, "%s\t: ", name);
    if (size == 8) {
      for (n = 0; n < 16; ++n) fprintf(stderr, "%.2x ", r.u8[n]);
    } else if (size == 16) {
      for (n = 0; n < 8; ++n) fprintf(stderr, "%.4x ", r.u16[n]);
    } else if (size == 32) {
      for (n = 0; n < 4; ++n) fprintf(stderr, "%.8x ", r.u32[n]);
    }
    fprintf(stderr, "\n");
  }
}

inline void PrintReg(const int32x4x2_t val, const std::string& name) {
  DebugRegisterQ r;
  vst1q_u32(r.u32, val.val[0]);
  const std::string name0 = absl::StrCat(name, ".val[0]").c_str();
  PrintVectQ(r, name0.c_str(), 32);
  vst1q_u32(r.u32, val.val[1]);
  const std::string name1 = absl::StrCat(name, ".val[1]").c_str();
  PrintVectQ(r, name1.c_str(), 32);
}

inline void PrintReg(const uint32x4_t val, const char* name) {
  DebugRegisterQ r;
  vst1q_u32(r.u32, val);
  PrintVectQ(r, name, 32);
}

inline void PrintReg(const uint32x2_t val, const char* name) {
  DebugRegister r;
  vst1_u32(r.u32, val);
  PrintVect(r, name, 32);
}

inline void PrintReg(const uint16x8_t val, const char* name) {
  DebugRegisterQ r;
  vst1q_u16(r.u16, val);
  PrintVectQ(r, name, 16);
}

inline void PrintReg(const uint16x4_t val, const char* name) {
  DebugRegister r;
  vst1_u16(r.u16, val);
  PrintVect(r, name, 16);
}

inline void PrintReg(const uint8x16_t val, const char* name) {
  DebugRegisterQ r;
  vst1q_u8(r.u8, val);
  PrintVectQ(r, name, 8);
}

inline void PrintReg(const uint8x8_t val, const char* name) {
  DebugRegister r;
  vst1_u8(r.u8, val);
  PrintVect(r, name, 8);
}

inline void PrintReg(const int32x4_t val, const char* name) {
  DebugRegisterQ r;
  vst1q_s32(r.i32, val);
  PrintVectQ(r, name, 32);
}

inline void PrintReg(const int32x2_t val, const char* name) {
  DebugRegister r;
  vst1_s32(r.i32, val);
  PrintVect(r, name, 32);
}

inline void PrintReg(const int16x8_t val, const char* name) {
  DebugRegisterQ r;
  vst1q_s16(r.i16, val);
  PrintVectQ(r, name, 16);
}

inline void PrintReg(const int16x4_t val, const char* name) {
  DebugRegister r;
  vst1_s16(r.i16, val);
  PrintVect(r, name, 16);
}

inline void PrintReg(const int8x16_t val, const char* name) {
  DebugRegisterQ r;
  vst1q_s8(r.i8, val);
  PrintVectQ(r, name, 8);
}

inline void PrintReg(const int8x8_t val, const char* name) {
  DebugRegister r;
  vst1_s8(r.i8, val);
  PrintVect(r, name, 8);
}

// Print an individual (non-vector) value in decimal format.
inline void PrintReg(const int x, const char* name) {
  if (kEnablePrintRegs) {
    printf("%s: %d\n", name, x);
  }
}

// Print an individual (non-vector) value in hexadecimal format.
inline void PrintHex(const int x, const char* name) {
  if (kEnablePrintRegs) {
    printf("%s: %x\n", name, x);
  }
}

#define PR(x) PrintReg(x, #x)
#define PD(x) PrintReg(x, #x)
#define PX(x) PrintHex(x, #x)

#endif  // 0

namespace libgav1 {
namespace dsp {

//------------------------------------------------------------------------------
// Load functions.

// Load 2 uint8_t values into |lane| * 2 and |lane| * 2 + 1.
template <int lane>
inline uint8x8_t Load2(const void* const buf, uint8x8_t val) {
  uint16_t temp;
  memcpy(&temp, buf, 2);
  return vreinterpret_u8_u16(
      vld1_lane_u16(&temp, vreinterpret_u16_u8(val), lane));
}

// Load 4 int8_t values into the low half of an int8x8_t register.
inline int8x8_t LoadLo4(const void* const buf, int8x8_t val) {
  int32_t temp;
  memcpy(&temp, buf, 4);
  return vreinterpret_s8_s32(vld1_lane_s32(&temp, vreinterpret_s32_s8(val), 0));
}

// Load 4 uint8_t values into the low half of a uint8x8_t register.
inline uint8x8_t LoadLo4(const void* const buf, uint8x8_t val) {
  uint32_t temp;
  memcpy(&temp, buf, 4);
  return vreinterpret_u8_u32(vld1_lane_u32(&temp, vreinterpret_u32_u8(val), 0));
}

// Load 4 uint8_t values into the high half of a uint8x8_t register.
inline uint8x8_t LoadHi4(const void* const buf, uint8x8_t val) {
  uint32_t temp;
  memcpy(&temp, buf, 4);
  return vreinterpret_u8_u32(vld1_lane_u32(&temp, vreinterpret_u32_u8(val), 1));
}

//------------------------------------------------------------------------------
// Store functions.

// Propagate type information to the compiler. Without this the compiler may
// assume the required alignment of the type (4 bytes in the case of uint32_t)
// and add alignment hints to the memory access.
template <typename T>
inline void ValueToMem(void* const buf, T val) {
  memcpy(buf, &val, sizeof(val));
}

// Store 4 int8_t values from the low half of an int8x8_t register.
inline void StoreLo4(void* const buf, const int8x8_t val) {
  ValueToMem<int32_t>(buf, vget_lane_s32(vreinterpret_s32_s8(val), 0));
}

// Store 4 uint8_t values from the low half of a uint8x8_t register.
inline void StoreLo4(void* const buf, const uint8x8_t val) {
  ValueToMem<uint32_t>(buf, vget_lane_u32(vreinterpret_u32_u8(val), 0));
}

// Store 4 uint8_t values from the high half of a uint8x8_t register.
inline void StoreHi4(void* const buf, const uint8x8_t val) {
  ValueToMem<uint32_t>(buf, vget_lane_u32(vreinterpret_u32_u8(val), 1));
}

// Store 2 uint8_t values from |lane| * 2 and |lane| * 2 + 1 of a uint8x8_t
// register.
template <int lane>
inline void Store2(void* const buf, const uint8x8_t val) {
  ValueToMem<uint16_t>(buf, vget_lane_u16(vreinterpret_u16_u8(val), lane));
}

// Store 2 uint16_t values from |lane| * 2 and |lane| * 2 + 1 of a uint16x8_t
// register.
template <int lane>
inline void Store2(void* const buf, const uint16x8_t val) {
  ValueToMem<uint32_t>(buf, vgetq_lane_u32(vreinterpretq_u32_u16(val), lane));
}

// Store 2 uint16_t values from |lane| * 2 and |lane| * 2 + 1 of a uint16x4_t
// register.
template <int lane>
inline void Store2(uint16_t* const buf, const uint16x4_t val) {
  ValueToMem<uint32_t>(buf, vget_lane_u32(vreinterpret_u32_u16(val), lane));
}

//------------------------------------------------------------------------------
// Bit manipulation.

// vshXX_n_XX() requires an immediate.
template <int shift>
inline uint8x8_t RightShift(const uint8x8_t vector) {
  return vreinterpret_u8_u64(vshr_n_u64(vreinterpret_u64_u8(vector), shift));
}

template <int shift>
inline int8x8_t RightShift(const int8x8_t vector) {
  return vreinterpret_s8_u64(vshr_n_u64(vreinterpret_u64_s8(vector), shift));
}

// Shim vqtbl1_u8 for armv7.
inline uint8x8_t VQTbl1U8(const uint8x16_t a, const uint8x8_t index) {
#if defined(__aarch64__)
  return vqtbl1_u8(a, index);
#else
  const uint8x8x2_t b = {vget_low_u8(a), vget_high_u8(a)};
  return vtbl2_u8(b, index);
#endif
}

// Shim vqtbl1_s8 for armv7.
inline int8x8_t VQTbl1S8(const int8x16_t a, const uint8x8_t index) {
#if defined(__aarch64__)
  return vqtbl1_s8(a, index);
#else
  const int8x8x2_t b = {vget_low_s8(a), vget_high_s8(a)};
  return vtbl2_s8(b, vreinterpret_s8_u8(index));
#endif
}

//------------------------------------------------------------------------------
// Interleave.

// vzipN is exclusive to A64.
inline uint8x8_t InterleaveLow8(const uint8x8_t a, const uint8x8_t b) {
#if defined(__aarch64__)
  return vzip1_u8(a, b);
#else
  // Discard |.val[1]|
  return vzip_u8(a, b).val[0];
#endif
}

inline uint8x8_t InterleaveLow32(const uint8x8_t a, const uint8x8_t b) {
#if defined(__aarch64__)
  return vreinterpret_u8_u32(
      vzip1_u32(vreinterpret_u32_u8(a), vreinterpret_u32_u8(b)));
#else
  // Discard |.val[1]|
  return vreinterpret_u8_u32(
      vzip_u32(vreinterpret_u32_u8(a), vreinterpret_u32_u8(b)).val[0]);
#endif
}

inline int8x8_t InterleaveLow32(const int8x8_t a, const int8x8_t b) {
#if defined(__aarch64__)
  return vreinterpret_s8_u32(
      vzip1_u32(vreinterpret_u32_s8(a), vreinterpret_u32_s8(b)));
#else
  // Discard |.val[1]|
  return vreinterpret_s8_u32(
      vzip_u32(vreinterpret_u32_s8(a), vreinterpret_u32_s8(b)).val[0]);
#endif
}

inline uint8x8_t InterleaveHigh32(const uint8x8_t a, const uint8x8_t b) {
#if defined(__aarch64__)
  return vreinterpret_u8_u32(
      vzip2_u32(vreinterpret_u32_u8(a), vreinterpret_u32_u8(b)));
#else
  // Discard |.val[0]|
  return vreinterpret_u8_u32(
      vzip_u32(vreinterpret_u32_u8(a), vreinterpret_u32_u8(b)).val[1]);
#endif
}

inline int8x8_t InterleaveHigh32(const int8x8_t a, const int8x8_t b) {
#if defined(__aarch64__)
  return vreinterpret_s8_u32(
      vzip2_u32(vreinterpret_u32_s8(a), vreinterpret_u32_s8(b)));
#else
  // Discard |.val[0]|
  return vreinterpret_s8_u32(
      vzip_u32(vreinterpret_u32_s8(a), vreinterpret_u32_s8(b)).val[1]);
#endif
}

//------------------------------------------------------------------------------
// Transpose.

// Transpose 32 bit elements such that:
// a: 00 01
// b: 02 03
// returns
// val[0]: 00 02
// val[1]: 01 03
inline uint8x8x2_t Interleave32(const uint8x8_t a, const uint8x8_t b) {
  const uint32x2_t a_32 = vreinterpret_u32_u8(a);
  const uint32x2_t b_32 = vreinterpret_u32_u8(b);
  const uint32x2x2_t c = vtrn_u32(a_32, b_32);
  const uint8x8x2_t d = {vreinterpret_u8_u32(c.val[0]),
                         vreinterpret_u8_u32(c.val[1])};
  return d;
}

// Swap high and low 32 bit elements.
inline uint8x8_t Transpose32(const uint8x8_t a) {
  const uint32x2_t b = vrev64_u32(vreinterpret_u32_u8(a));
  return vreinterpret_u8_u32(b);
}

// Implement vtrnq_s64().
// Input:
// a0: 00 01 02 03 04 05 06 07
// a1: 16 17 18 19 20 21 22 23
// Output:
// b0.val[0]: 00 01 02 03 16 17 18 19
// b0.val[1]: 04 05 06 07 20 21 22 23
inline int16x8x2_t VtrnqS64(int32x4_t a0, int32x4_t a1) {
  int16x8x2_t b0;
  b0.val[0] = vcombine_s16(vreinterpret_s16_s32(vget_low_s32(a0)),
                           vreinterpret_s16_s32(vget_low_s32(a1)));
  b0.val[1] = vcombine_s16(vreinterpret_s16_s32(vget_high_s32(a0)),
                           vreinterpret_s16_s32(vget_high_s32(a1)));
  return b0;
}

inline uint16x8x2_t VtrnqU64(uint32x4_t a0, uint32x4_t a1) {
  uint16x8x2_t b0;
  b0.val[0] = vcombine_u16(vreinterpret_u16_u32(vget_low_u32(a0)),
                           vreinterpret_u16_u32(vget_low_u32(a1)));
  b0.val[1] = vcombine_u16(vreinterpret_u16_u32(vget_high_u32(a0)),
                           vreinterpret_u16_u32(vget_high_u32(a1)));
  return b0;
}

// Input:
// a: 00 01 02 03 10 11 12 13
// b: 20 21 22 23 30 31 32 33
// Output:
// Note that columns [1] and [2] are transposed.
// a: 00 10 20 30 02 12 22 32
// b: 01 11 21 31 03 13 23 33
inline void Transpose4x4(uint8x8_t* a, uint8x8_t* b) {
  const uint16x4x2_t c =
      vtrn_u16(vreinterpret_u16_u8(*a), vreinterpret_u16_u8(*b));
  const uint32x2x2_t d =
      vtrn_u32(vreinterpret_u32_u16(c.val[0]), vreinterpret_u32_u16(c.val[1]));
  const uint8x8x2_t e =
      vtrn_u8(vreinterpret_u8_u32(d.val[0]), vreinterpret_u8_u32(d.val[1]));
  *a = e.val[0];
  *b = e.val[1];
}

// Reversible if the x4 values are packed next to each other.
// x4 input / x8 output:
// a0: 00 01 02 03 40 41 42 43 44
// a1: 10 11 12 13 50 51 52 53 54
// a2: 20 21 22 23 60 61 62 63 64
// a3: 30 31 32 33 70 71 72 73 74
// x8 input / x4 output:
// a0: 00 10 20 30 40 50 60 70
// a1: 01 11 21 31 41 51 61 71
// a2: 02 12 22 32 42 52 62 72
// a3: 03 13 23 33 43 53 63 73
inline void Transpose8x4(uint8x8_t* a0, uint8x8_t* a1, uint8x8_t* a2,
                         uint8x8_t* a3) {
  const uint8x8x2_t b0 = vtrn_u8(*a0, *a1);
  const uint8x8x2_t b1 = vtrn_u8(*a2, *a3);

  const uint16x4x2_t c0 =
      vtrn_u16(vreinterpret_u16_u8(b0.val[0]), vreinterpret_u16_u8(b1.val[0]));
  const uint16x4x2_t c1 =
      vtrn_u16(vreinterpret_u16_u8(b0.val[1]), vreinterpret_u16_u8(b1.val[1]));

  *a0 = vreinterpret_u8_u16(c0.val[0]);
  *a1 = vreinterpret_u8_u16(c1.val[0]);
  *a2 = vreinterpret_u8_u16(c0.val[1]);
  *a3 = vreinterpret_u8_u16(c1.val[1]);
}

// Input:
// a[0]: 00 01 02 03 04 05 06 07
// a[1]: 10 11 12 13 14 15 16 17
// a[2]: 20 21 22 23 24 25 26 27
// a[3]: 30 31 32 33 34 35 36 37
// a[4]: 40 41 42 43 44 45 46 47
// a[5]: 50 51 52 53 54 55 56 57
// a[6]: 60 61 62 63 64 65 66 67
// a[7]: 70 71 72 73 74 75 76 77

// Output:
// a[0]: 00 10 20 30 40 50 60 70
// a[1]: 01 11 21 31 41 51 61 71
// a[2]: 02 12 22 32 42 52 62 72
// a[3]: 03 13 23 33 43 53 63 73
// a[4]: 04 14 24 34 44 54 64 74
// a[5]: 05 15 25 35 45 55 65 75
// a[6]: 06 16 26 36 46 56 66 76
// a[7]: 07 17 27 37 47 57 67 77
inline void Transpose8x8(int8x8_t a[8]) {
  // Swap 8 bit elements. Goes from:
  // a[0]: 00 01 02 03 04 05 06 07
  // a[1]: 10 11 12 13 14 15 16 17
  // a[2]: 20 21 22 23 24 25 26 27
  // a[3]: 30 31 32 33 34 35 36 37
  // a[4]: 40 41 42 43 44 45 46 47
  // a[5]: 50 51 52 53 54 55 56 57
  // a[6]: 60 61 62 63 64 65 66 67
  // a[7]: 70 71 72 73 74 75 76 77
  // to:
  // b0.val[0]: 00 10 02 12 04 14 06 16  40 50 42 52 44 54 46 56
  // b0.val[1]: 01 11 03 13 05 15 07 17  41 51 43 53 45 55 47 57
  // b1.val[0]: 20 30 22 32 24 34 26 36  60 70 62 72 64 74 66 76
  // b1.val[1]: 21 31 23 33 25 35 27 37  61 71 63 73 65 75 67 77
  const int8x16x2_t b0 =
      vtrnq_s8(vcombine_s8(a[0], a[4]), vcombine_s8(a[1], a[5]));
  const int8x16x2_t b1 =
      vtrnq_s8(vcombine_s8(a[2], a[6]), vcombine_s8(a[3], a[7]));

  // Swap 16 bit elements resulting in:
  // c0.val[0]: 00 10 20 30 04 14 24 34  40 50 60 70 44 54 64 74
  // c0.val[1]: 02 12 22 32 06 16 26 36  42 52 62 72 46 56 66 76
  // c1.val[0]: 01 11 21 31 05 15 25 35  41 51 61 71 45 55 65 75
  // c1.val[1]: 03 13 23 33 07 17 27 37  43 53 63 73 47 57 67 77
  const int16x8x2_t c0 = vtrnq_s16(vreinterpretq_s16_s8(b0.val[0]),
                                   vreinterpretq_s16_s8(b1.val[0]));
  const int16x8x2_t c1 = vtrnq_s16(vreinterpretq_s16_s8(b0.val[1]),
                                   vreinterpretq_s16_s8(b1.val[1]));

  // Unzip 32 bit elements resulting in:
  // d0.val[0]: 00 10 20 30 40 50 60 70  01 11 21 31 41 51 61 71
  // d0.val[1]: 04 14 24 34 44 54 64 74  05 15 25 35 45 55 65 75
  // d1.val[0]: 02 12 22 32 42 52 62 72  03 13 23 33 43 53 63 73
  // d1.val[1]: 06 16 26 36 46 56 66 76  07 17 27 37 47 57 67 77
  const int32x4x2_t d0 = vuzpq_s32(vreinterpretq_s32_s16(c0.val[0]),
                                   vreinterpretq_s32_s16(c1.val[0]));
  const int32x4x2_t d1 = vuzpq_s32(vreinterpretq_s32_s16(c0.val[1]),
                                   vreinterpretq_s32_s16(c1.val[1]));

  a[0] = vreinterpret_s8_s32(vget_low_s32(d0.val[0]));
  a[1] = vreinterpret_s8_s32(vget_high_s32(d0.val[0]));
  a[2] = vreinterpret_s8_s32(vget_low_s32(d1.val[0]));
  a[3] = vreinterpret_s8_s32(vget_high_s32(d1.val[0]));
  a[4] = vreinterpret_s8_s32(vget_low_s32(d0.val[1]));
  a[5] = vreinterpret_s8_s32(vget_high_s32(d0.val[1]));
  a[6] = vreinterpret_s8_s32(vget_low_s32(d1.val[1]));
  a[7] = vreinterpret_s8_s32(vget_high_s32(d1.val[1]));
}

// Unsigned.
inline void Transpose8x8(uint8x8_t a[8]) {
  const uint8x16x2_t b0 =
      vtrnq_u8(vcombine_u8(a[0], a[4]), vcombine_u8(a[1], a[5]));
  const uint8x16x2_t b1 =
      vtrnq_u8(vcombine_u8(a[2], a[6]), vcombine_u8(a[3], a[7]));

  const uint16x8x2_t c0 = vtrnq_u16(vreinterpretq_u16_u8(b0.val[0]),
                                    vreinterpretq_u16_u8(b1.val[0]));
  const uint16x8x2_t c1 = vtrnq_u16(vreinterpretq_u16_u8(b0.val[1]),
                                    vreinterpretq_u16_u8(b1.val[1]));

  const uint32x4x2_t d0 = vuzpq_u32(vreinterpretq_u32_u16(c0.val[0]),
                                    vreinterpretq_u32_u16(c1.val[0]));
  const uint32x4x2_t d1 = vuzpq_u32(vreinterpretq_u32_u16(c0.val[1]),
                                    vreinterpretq_u32_u16(c1.val[1]));

  a[0] = vreinterpret_u8_u32(vget_low_u32(d0.val[0]));
  a[1] = vreinterpret_u8_u32(vget_high_u32(d0.val[0]));
  a[2] = vreinterpret_u8_u32(vget_low_u32(d1.val[0]));
  a[3] = vreinterpret_u8_u32(vget_high_u32(d1.val[0]));
  a[4] = vreinterpret_u8_u32(vget_low_u32(d0.val[1]));
  a[5] = vreinterpret_u8_u32(vget_high_u32(d0.val[1]));
  a[6] = vreinterpret_u8_u32(vget_low_u32(d1.val[1]));
  a[7] = vreinterpret_u8_u32(vget_high_u32(d1.val[1]));
}

// Input:
// a0: 00 01 02 03 04 05 06 07
// a1: 10 11 12 13 14 15 16 17
// a2: 20 21 22 23 24 25 26 27
// a3: 30 31 32 33 34 35 36 37
// a4: 40 41 42 43 44 45 46 47
// a5: 50 51 52 53 54 55 56 57
// a6: 60 61 62 63 64 65 66 67
// a7: 70 71 72 73 74 75 76 77

// Output:
// a0: 00 10 20 30 40 50 60 70
// a1: 01 11 21 31 41 51 61 71
// a2: 02 12 22 32 42 52 62 72
// a3: 03 13 23 33 43 53 63 73
// a4: 04 14 24 34 44 54 64 74
// a5: 05 15 25 35 45 55 65 75
// a6: 06 16 26 36 46 56 66 76
// a7: 07 17 27 37 47 57 67 77
inline void Transpose8x8(int16x8_t* a0, int16x8_t* a1, int16x8_t* a2,
                         int16x8_t* a3, int16x8_t* a4, int16x8_t* a5,
                         int16x8_t* a6, int16x8_t* a7) {
  const int16x8x2_t b0 = vtrnq_s16(*a0, *a1);
  const int16x8x2_t b1 = vtrnq_s16(*a2, *a3);
  const int16x8x2_t b2 = vtrnq_s16(*a4, *a5);
  const int16x8x2_t b3 = vtrnq_s16(*a6, *a7);

  const int32x4x2_t c0 = vtrnq_s32(vreinterpretq_s32_s16(b0.val[0]),
                                   vreinterpretq_s32_s16(b1.val[0]));
  const int32x4x2_t c1 = vtrnq_s32(vreinterpretq_s32_s16(b0.val[1]),
                                   vreinterpretq_s32_s16(b1.val[1]));
  const int32x4x2_t c2 = vtrnq_s32(vreinterpretq_s32_s16(b2.val[0]),
                                   vreinterpretq_s32_s16(b3.val[0]));
  const int32x4x2_t c3 = vtrnq_s32(vreinterpretq_s32_s16(b2.val[1]),
                                   vreinterpretq_s32_s16(b3.val[1]));

  const int16x8x2_t d0 = VtrnqS64(c0.val[0], c2.val[0]);
  const int16x8x2_t d1 = VtrnqS64(c1.val[0], c3.val[0]);
  const int16x8x2_t d2 = VtrnqS64(c0.val[1], c2.val[1]);
  const int16x8x2_t d3 = VtrnqS64(c1.val[1], c3.val[1]);

  *a0 = d0.val[0];
  *a1 = d1.val[0];
  *a2 = d2.val[0];
  *a3 = d3.val[0];
  *a4 = d0.val[1];
  *a5 = d1.val[1];
  *a6 = d2.val[1];
  *a7 = d3.val[1];
}

// Input:
// a0: 00 01 02 03 04 05 06 07
// a1: 10 11 12 13 14 15 16 17
// a2: 20 21 22 23 24 25 26 27
// a3: 30 31 32 33 34 35 36 37
// a4: 40 41 42 43 44 45 46 47
// a5: 50 51 52 53 54 55 56 57
// a6: 60 61 62 63 64 65 66 67
// a7: 70 71 72 73 74 75 76 77

// Output:
// a0: 00 10 20 30 40 50 60 70
// a1: 01 11 21 31 41 51 61 71
// a2: 02 12 22 32 42 52 62 72
// a3: 03 13 23 33 43 53 63 73
// a4: 04 14 24 34 44 54 64 74
// a5: 05 15 25 35 45 55 65 75
// a6: 06 16 26 36 46 56 66 76
// a7: 07 17 27 37 47 57 67 77
// TODO(johannkoenig): Switch users of the above transpose to this one.
inline void Transpose8x8(int16x8_t a[8]) {
  const int16x8x2_t b0 = vtrnq_s16(a[0], a[1]);
  const int16x8x2_t b1 = vtrnq_s16(a[2], a[3]);
  const int16x8x2_t b2 = vtrnq_s16(a[4], a[5]);
  const int16x8x2_t b3 = vtrnq_s16(a[6], a[7]);

  const int32x4x2_t c0 = vtrnq_s32(vreinterpretq_s32_s16(b0.val[0]),
                                   vreinterpretq_s32_s16(b1.val[0]));
  const int32x4x2_t c1 = vtrnq_s32(vreinterpretq_s32_s16(b0.val[1]),
                                   vreinterpretq_s32_s16(b1.val[1]));
  const int32x4x2_t c2 = vtrnq_s32(vreinterpretq_s32_s16(b2.val[0]),
                                   vreinterpretq_s32_s16(b3.val[0]));
  const int32x4x2_t c3 = vtrnq_s32(vreinterpretq_s32_s16(b2.val[1]),
                                   vreinterpretq_s32_s16(b3.val[1]));

  const int16x8x2_t d0 = VtrnqS64(c0.val[0], c2.val[0]);
  const int16x8x2_t d1 = VtrnqS64(c1.val[0], c3.val[0]);
  const int16x8x2_t d2 = VtrnqS64(c0.val[1], c2.val[1]);
  const int16x8x2_t d3 = VtrnqS64(c1.val[1], c3.val[1]);

  a[0] = d0.val[0];
  a[1] = d1.val[0];
  a[2] = d2.val[0];
  a[3] = d3.val[0];
  a[4] = d0.val[1];
  a[5] = d1.val[1];
  a[6] = d2.val[1];
  a[7] = d3.val[1];
}

// Unsigned.
inline void Transpose8x8(uint16x8_t a[8]) {
  const uint16x8x2_t b0 = vtrnq_u16(a[0], a[1]);
  const uint16x8x2_t b1 = vtrnq_u16(a[2], a[3]);
  const uint16x8x2_t b2 = vtrnq_u16(a[4], a[5]);
  const uint16x8x2_t b3 = vtrnq_u16(a[6], a[7]);

  const uint32x4x2_t c0 = vtrnq_u32(vreinterpretq_u32_u16(b0.val[0]),
                                    vreinterpretq_u32_u16(b1.val[0]));
  const uint32x4x2_t c1 = vtrnq_u32(vreinterpretq_u32_u16(b0.val[1]),
                                    vreinterpretq_u32_u16(b1.val[1]));
  const uint32x4x2_t c2 = vtrnq_u32(vreinterpretq_u32_u16(b2.val[0]),
                                    vreinterpretq_u32_u16(b3.val[0]));
  const uint32x4x2_t c3 = vtrnq_u32(vreinterpretq_u32_u16(b2.val[1]),
                                    vreinterpretq_u32_u16(b3.val[1]));

  const uint16x8x2_t d0 = VtrnqU64(c0.val[0], c2.val[0]);
  const uint16x8x2_t d1 = VtrnqU64(c1.val[0], c3.val[0]);
  const uint16x8x2_t d2 = VtrnqU64(c0.val[1], c2.val[1]);
  const uint16x8x2_t d3 = VtrnqU64(c1.val[1], c3.val[1]);

  a[0] = d0.val[0];
  a[1] = d1.val[0];
  a[2] = d2.val[0];
  a[3] = d3.val[0];
  a[4] = d0.val[1];
  a[5] = d1.val[1];
  a[6] = d2.val[1];
  a[7] = d3.val[1];
}

// Input:
// i0: 00 01 02 03 04 05 06 07 08 09 0a 0b 0c 0d 0e 0f
// i1: 10 11 12 13 14 15 16 17 18 19 1a 1b 1c 1d 1e 1f
// i2: 20 21 22 23 24 25 26 27 28 29 2a 2b 2c 2d 2e 2f
// i3: 30 31 32 33 34 35 36 37 38 39 3a 3b 3c 3d 3e 3f
// i4: 40 41 42 43 44 45 46 47 48 49 4a 4b 4c 4d 4e 4f
// i5: 50 51 52 53 54 55 56 57 58 59 5a 5b 5c 5d 5e 5f
// i6: 60 61 62 63 64 65 66 67 68 69 6a 6b 6c 6d 6e 6f
// i7: 70 71 72 73 74 75 76 77 78 79 7a 7b 7c 7d 7e 7f

// Output:
// o00: 00 10 20 30 40 50 60 70
// o01: 01 11 21 31 41 51 61 71
// o02: 02 12 22 32 42 52 62 72
// o03: 03 13 23 33 43 53 63 73
// o04: 04 14 24 34 44 54 64 74
// o05: 05 15 25 35 45 55 65 75
// o06: 06 16 26 36 46 56 66 76
// o07: 07 17 27 37 47 57 67 77
// o08: 08 18 28 38 48 58 68 78
// o09: 09 19 29 39 49 59 69 79
// o0a: 0a 1a 2a 3a 4a 5a 6a 7a
// o0b: 0b 1b 2b 3b 4b 5b 6b 7b
// o0c: 0c 1c 2c 3c 4c 5c 6c 7c
// o0d: 0d 1d 2d 3d 4d 5d 6d 7d
// o0e: 0e 1e 2e 3e 4e 5e 6e 7e
// o0f: 0f 1f 2f 3f 4f 5f 6f 7f
inline void Transpose16x8(const uint8x16_t i0, const uint8x16_t i1,
                          const uint8x16_t i2, const uint8x16_t i3,
                          const uint8x16_t i4, const uint8x16_t i5,
                          const uint8x16_t i6, const uint8x16_t i7,
                          uint8x8_t* o00, uint8x8_t* o01, uint8x8_t* o02,
                          uint8x8_t* o03, uint8x8_t* o04, uint8x8_t* o05,
                          uint8x8_t* o06, uint8x8_t* o07, uint8x8_t* o08,
                          uint8x8_t* o09, uint8x8_t* o10, uint8x8_t* o11,
                          uint8x8_t* o12, uint8x8_t* o13, uint8x8_t* o14,
                          uint8x8_t* o15) {
  const uint8x16x2_t b0 = vtrnq_u8(i0, i1);
  const uint8x16x2_t b1 = vtrnq_u8(i2, i3);
  const uint8x16x2_t b2 = vtrnq_u8(i4, i5);
  const uint8x16x2_t b3 = vtrnq_u8(i6, i7);

  const uint16x8x2_t c0 = vtrnq_u16(vreinterpretq_u16_u8(b0.val[0]),
                                    vreinterpretq_u16_u8(b1.val[0]));
  const uint16x8x2_t c1 = vtrnq_u16(vreinterpretq_u16_u8(b0.val[1]),
                                    vreinterpretq_u16_u8(b1.val[1]));
  const uint16x8x2_t c2 = vtrnq_u16(vreinterpretq_u16_u8(b2.val[0]),
                                    vreinterpretq_u16_u8(b3.val[0]));
  const uint16x8x2_t c3 = vtrnq_u16(vreinterpretq_u16_u8(b2.val[1]),
                                    vreinterpretq_u16_u8(b3.val[1]));

  const uint32x4x2_t d0 = vtrnq_u32(vreinterpretq_u32_u16(c0.val[0]),
                                    vreinterpretq_u32_u16(c2.val[0]));
  const uint32x4x2_t d1 = vtrnq_u32(vreinterpretq_u32_u16(c0.val[1]),
                                    vreinterpretq_u32_u16(c2.val[1]));
  const uint32x4x2_t d2 = vtrnq_u32(vreinterpretq_u32_u16(c1.val[0]),
                                    vreinterpretq_u32_u16(c3.val[0]));
  const uint32x4x2_t d3 = vtrnq_u32(vreinterpretq_u32_u16(c1.val[1]),
                                    vreinterpretq_u32_u16(c3.val[1]));

  *o00 = vget_low_u8(vreinterpretq_u8_u32(d0.val[0]));
  *o01 = vget_low_u8(vreinterpretq_u8_u32(d2.val[0]));
  *o02 = vget_low_u8(vreinterpretq_u8_u32(d1.val[0]));
  *o03 = vget_low_u8(vreinterpretq_u8_u32(d3.val[0]));
  *o04 = vget_low_u8(vreinterpretq_u8_u32(d0.val[1]));
  *o05 = vget_low_u8(vreinterpretq_u8_u32(d2.val[1]));
  *o06 = vget_low_u8(vreinterpretq_u8_u32(d1.val[1]));
  *o07 = vget_low_u8(vreinterpretq_u8_u32(d3.val[1]));
  *o08 = vget_high_u8(vreinterpretq_u8_u32(d0.val[0]));
  *o09 = vget_high_u8(vreinterpretq_u8_u32(d2.val[0]));
  *o10 = vget_high_u8(vreinterpretq_u8_u32(d1.val[0]));
  *o11 = vget_high_u8(vreinterpretq_u8_u32(d3.val[0]));
  *o12 = vget_high_u8(vreinterpretq_u8_u32(d0.val[1]));
  *o13 = vget_high_u8(vreinterpretq_u8_u32(d2.val[1]));
  *o14 = vget_high_u8(vreinterpretq_u8_u32(d1.val[1]));
  *o15 = vget_high_u8(vreinterpretq_u8_u32(d3.val[1]));
}

// TODO(johannkoenig): Replace usage of the above transpose with this one.
inline void Transpose16x8(const uint8x16_t input[8], uint8x8_t output[16]) {
  const uint8x16x2_t b0 = vtrnq_u8(input[0], input[1]);
  const uint8x16x2_t b1 = vtrnq_u8(input[2], input[3]);
  const uint8x16x2_t b2 = vtrnq_u8(input[4], input[5]);
  const uint8x16x2_t b3 = vtrnq_u8(input[6], input[7]);

  const uint16x8x2_t c0 = vtrnq_u16(vreinterpretq_u16_u8(b0.val[0]),
                                    vreinterpretq_u16_u8(b1.val[0]));
  const uint16x8x2_t c1 = vtrnq_u16(vreinterpretq_u16_u8(b0.val[1]),
                                    vreinterpretq_u16_u8(b1.val[1]));
  const uint16x8x2_t c2 = vtrnq_u16(vreinterpretq_u16_u8(b2.val[0]),
                                    vreinterpretq_u16_u8(b3.val[0]));
  const uint16x8x2_t c3 = vtrnq_u16(vreinterpretq_u16_u8(b2.val[1]),
                                    vreinterpretq_u16_u8(b3.val[1]));

  const uint32x4x2_t d0 = vtrnq_u32(vreinterpretq_u32_u16(c0.val[0]),
                                    vreinterpretq_u32_u16(c2.val[0]));
  const uint32x4x2_t d1 = vtrnq_u32(vreinterpretq_u32_u16(c0.val[1]),
                                    vreinterpretq_u32_u16(c2.val[1]));
  const uint32x4x2_t d2 = vtrnq_u32(vreinterpretq_u32_u16(c1.val[0]),
                                    vreinterpretq_u32_u16(c3.val[0]));
  const uint32x4x2_t d3 = vtrnq_u32(vreinterpretq_u32_u16(c1.val[1]),
                                    vreinterpretq_u32_u16(c3.val[1]));

  output[0] = vget_low_u8(vreinterpretq_u8_u32(d0.val[0]));
  output[1] = vget_low_u8(vreinterpretq_u8_u32(d2.val[0]));
  output[2] = vget_low_u8(vreinterpretq_u8_u32(d1.val[0]));
  output[3] = vget_low_u8(vreinterpretq_u8_u32(d3.val[0]));
  output[4] = vget_low_u8(vreinterpretq_u8_u32(d0.val[1]));
  output[5] = vget_low_u8(vreinterpretq_u8_u32(d2.val[1]));
  output[6] = vget_low_u8(vreinterpretq_u8_u32(d1.val[1]));
  output[7] = vget_low_u8(vreinterpretq_u8_u32(d3.val[1]));
  output[8] = vget_high_u8(vreinterpretq_u8_u32(d0.val[0]));
  output[9] = vget_high_u8(vreinterpretq_u8_u32(d2.val[0]));
  output[10] = vget_high_u8(vreinterpretq_u8_u32(d1.val[0]));
  output[11] = vget_high_u8(vreinterpretq_u8_u32(d3.val[0]));
  output[12] = vget_high_u8(vreinterpretq_u8_u32(d0.val[1]));
  output[13] = vget_high_u8(vreinterpretq_u8_u32(d2.val[1]));
  output[14] = vget_high_u8(vreinterpretq_u8_u32(d1.val[1]));
  output[15] = vget_high_u8(vreinterpretq_u8_u32(d3.val[1]));
}

}  // namespace dsp
}  // namespace libgav1

#endif  // LIBGAV1_ENABLE_NEON
#endif  // LIBGAV1_SRC_DSP_ARM_COMMON_NEON_H_

/*
 * Copyright 2008 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

// Fast log & pow functions:
//   Jim Blinn's Corner
//   IEEE Computer Graphics And Applications
//   July / August 1997
//
// Gain & bias functions:
//   Texturing & Modeling: A Procedural Approach
//   David S. Ebert [et al.]
//   1994 Academic Press, Inc.
//
// There's a lot of magic going on here, it is highly recommended to
// read the references above for a full explanation.
//
// Note: The functions in this file should *not* be used if numerical
// precision is required.

#include <math.h>

#define FASTMATH_INLINE __attribute__((no_instrument_function, always_inline))

// magic constants used for various floating point manipulations
const float kMagicFloatToInt = (1 << 23);
const int kOneAsInt = 0x3F800000;
const float kScaleUp = static_cast<float>(0x00800000);
const float kScaleDown = 1.0f / kScaleUp;

// union to switch from float to int and back without changing the bits
union Reinterpret {
  int i;
  float f;
};

FASTMATH_INLINE float Clamp(float f);
inline float Clamp(float f) {
  if (f < 0.0f) return 0.0f;
  if (f > 1.0f) return 1.0f;
  return f;
}

FASTMATH_INLINE int ToInt(float f);
inline int ToInt(float f) {
  Reinterpret x;
  // force integer part into lower bits of mantissa
  x.f = f + kMagicFloatToInt;
  // return lower bits of mantissa
  return x.i & 0x3FFFFF;
}

FASTMATH_INLINE int AsInt(float f);
inline int AsInt(float f) {
  Reinterpret b;
  b.f = f;
  return b.i;
}

FASTMATH_INLINE float AsFloat(int i);
inline float AsFloat(int i) {
  Reinterpret b;
  b.i = i;
  return b.f;
}

FASTMATH_INLINE float FastLog2(float x);
inline float FastLog2(float x) {
  return static_cast<float>(AsInt(x) - kOneAsInt) * kScaleDown;
}

FASTMATH_INLINE float FastPower(float x, float p);
inline float FastPower(float x, float p) {
  return AsFloat(static_cast<int>(p * AsInt(x) +
    (1.0f - p) * kOneAsInt));
}

FASTMATH_INLINE float FastBias(float b, float x);
inline float FastBias(float b, float x) {
  return FastPower(x, FastLog2(b) / FastLog2(0.5f));
}

FASTMATH_INLINE float FastGain(float g, float x);
inline float FastGain(float g, float x) {
  return (x < 0.5f) ?
      FastBias(1.0f - g, 2.0f * x) * 0.5f :
      1.0f - FastBias(1.0f - g, 2.0f - 2.0f * x) * 0.5f;
}


/*
 * Copyright 2008, Google Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
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


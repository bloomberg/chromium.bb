// Auto-generated file. Do not edit!
//   Template: src/f32-vbinary/vopc-scalar.c.in
//   Generator: tools/xngen
//
// Copyright 2019 Google LLC
//
// This source code is licensed under the BSD-style license found in the
// LICENSE file in the root directory of this source tree.

#include <assert.h>

#include <xnnpack/common.h>
#include <xnnpack/math.h>
#include <xnnpack/vbinary.h>


void xnn_f32_vrdivc_relu_ukernel__scalar_x4(
    size_t n,
    const float* a,
    const float* b,
    float* y,
    const union xnn_f32_relu_params params[restrict XNN_MIN_ELEMENTS(1)])
{
  assert(n != 0);
  assert(n % sizeof(float) == 0);
  assert(a != NULL);
  assert(b != NULL);
  assert(y != NULL);


  const float vb = *b;
  for (; n >= 4 * sizeof(float); n -= 4 * sizeof(float)) {
    const float va0 = a[0];
    const float va1 = a[1];
    const float va2 = a[2];
    const float va3 = a[3];
    a += 4;

    float vy0 = vb / va0;
    float vy1 = vb / va1;
    float vy2 = vb / va2;
    float vy3 = vb / va3;


    vy0 = math_max_f32(vy0, 0.0f);
    vy1 = math_max_f32(vy1, 0.0f);
    vy2 = math_max_f32(vy2, 0.0f);
    vy3 = math_max_f32(vy3, 0.0f);

    y[0] = vy0;
    y[1] = vy1;
    y[2] = vy2;
    y[3] = vy3;
    y += 4;
  }
  if XNN_UNLIKELY(n != 0) {
    do {
      const float va = *a++;
      float vy = vb / va;
      vy = math_max_f32(vy, 0.0f);
      *y++ = vy;
      n -= sizeof(float);
    } while (n != 0);
  }
}

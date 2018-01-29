/*
 * Copyright (c) 2016, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "third_party/googletest/src/googletest/include/gtest/gtest.h"

#include "./aom_dsp_rtcd.h"
#include "test/acm_random.h"
#include "aom/aom_integer.h"
#include "aom_ports/msvc.h"  // for round()

using libaom_test::ACMRandom;

namespace {

void reference_dct_1d(double input[8], double output[8]) {
  const double kPi = 3.141592653589793238462643383279502884;
  const double kInvSqrt2 = 0.707106781186547524400844362104;
  for (int k = 0; k < 8; k++) {
    output[k] = 0.0;
    for (int n = 0; n < 8; n++)
      output[k] += input[n] * cos(kPi * (2 * n + 1) * k / 16.0);
    if (k == 0) output[k] = output[k] * kInvSqrt2;
  }
}

void reference_dct_2d(int16_t input[64], double output[64]) {
  // First transform columns
  for (int i = 0; i < 8; ++i) {
    double temp_in[8], temp_out[8];
    for (int j = 0; j < 8; ++j) temp_in[j] = input[j * 8 + i];
    reference_dct_1d(temp_in, temp_out);
    for (int j = 0; j < 8; ++j) output[j * 8 + i] = temp_out[j];
  }
  // Then transform rows
  for (int i = 0; i < 8; ++i) {
    double temp_in[8], temp_out[8];
    for (int j = 0; j < 8; ++j) temp_in[j] = output[j + i * 8];
    reference_dct_1d(temp_in, temp_out);
    for (int j = 0; j < 8; ++j) output[j + i * 8] = temp_out[j];
  }
  // Scale by some magic number
  for (int i = 0; i < 64; ++i) output[i] *= 2;
}

}  // namespace

// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/skia_color_space_util.h"

#include <algorithm>
#include <cmath>

namespace gfx {

namespace {

const float kEpsilon = 1.f / 256.f;
}

float EvalSkTransferFn(const SkColorSpaceTransferFn& fn, float x) {
  if (x < 0.f)
    return 0.f;
  if (x < fn.fD)
    return fn.fC * x + fn.fF;
  return std::pow(fn.fA * x + fn.fB, fn.fG) + fn.fE;
}

SkColorSpaceTransferFn SkTransferFnInverse(const SkColorSpaceTransferFn& fn) {
  SkColorSpaceTransferFn fn_inv = {0};
  if (fn.fA > 0 && fn.fG > 0) {
    double a_to_the_g = std::pow(fn.fA, fn.fG);
    fn_inv.fA = 1.f / a_to_the_g;
    fn_inv.fB = -fn.fE / a_to_the_g;
    fn_inv.fG = 1.f / fn.fG;
  }
  fn_inv.fD = fn.fC * fn.fD + fn.fF;
  fn_inv.fE = -fn.fB / fn.fA;
  if (fn.fC != 0) {
    fn_inv.fC = 1.f / fn.fC;
    fn_inv.fF = -fn.fF / fn.fC;
  }
  return fn_inv;
}

bool SkMatrixIsApproximatelyIdentity(const SkMatrix44& m) {
  for (int i = 0; i < 4; ++i) {
    for (int j = 0; j < 4; ++j) {
      float identity_value = i == j ? 1 : 0;
      float value = m.get(i, j);
      if (std::abs(identity_value - value) > kEpsilon)
        return false;
    }
  }
  return true;
}

}  // namespace gfx

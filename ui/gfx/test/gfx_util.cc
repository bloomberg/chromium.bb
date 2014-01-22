// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/test/gfx_util.h"

#include <iomanip>
#include <sstream>
#include <string>

namespace gfx {

namespace {

std::string ColorAsString(SkColor color) {
  std::ostringstream stream;
  stream << std::hex << std::uppercase << "#" << std::setfill('0')
         << std::setw(2) << SkColorGetA(color)
         << std::setw(2) << SkColorGetR(color)
         << std::setw(2) << SkColorGetG(color)
         << std::setw(2) << SkColorGetB(color);
  return stream.str();
}

bool FloatAlmostEqual(float a, float b) {
  // FloatLE is the gtest predicate for less than or almost equal to.
  return ::testing::FloatLE("a", "b", a, b) &&
         ::testing::FloatLE("b", "a", b, a);
}

}  // namespace

::testing::AssertionResult AssertBoxFloatEqual(const char* lhs_expr,
                                               const char* rhs_expr,
                                               const BoxF& lhs,
                                               const BoxF& rhs) {
  if (FloatAlmostEqual(lhs.x(), rhs.x()) &&
      FloatAlmostEqual(lhs.y(), rhs.y()) &&
      FloatAlmostEqual(lhs.z(), rhs.z()) &&
      FloatAlmostEqual(lhs.width(), rhs.width()) &&
      FloatAlmostEqual(lhs.height(), rhs.height()) &&
      FloatAlmostEqual(lhs.depth(), rhs.depth())) {
    return ::testing::AssertionSuccess();
  }
  return ::testing::AssertionFailure() << "Value of: " << rhs_expr
                                       << "\n  Actual: " << rhs.ToString()
                                       << "\nExpected: " << lhs_expr
                                       << "\nWhich is: " << lhs.ToString();
}

::testing::AssertionResult AssertSkColorsEqual(const char* lhs_expr,
                                               const char* rhs_expr,
                                               SkColor lhs,
                                               SkColor rhs) {
  if (lhs == rhs) {
    return ::testing::AssertionSuccess();
  }
  return ::testing::AssertionFailure() << "Value of: " << rhs_expr
                                       << "\n  Actual: " << ColorAsString(rhs)
                                       << "\nExpected: " << lhs_expr
                                       << "\nWhich is: " << ColorAsString(lhs);
}

}  // namespace gfx

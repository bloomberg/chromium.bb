// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/test/color_util.h"

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

}  // namespace

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

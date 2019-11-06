// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ZUCCHINI_TYPE_ZTF_H_
#define COMPONENTS_ZUCCHINI_TYPE_ZTF_H_

#include <stddef.h>
#include <stdint.h>

namespace zucchini {

namespace ztf {

typedef int16_t dim_t;

// A exclusive upper bound on number of lines and/or columns. Throughout the ZTF
// code a dimension (dim) refers to a block of 1-3 digits which contain a line
// or column number.
enum : size_t { kMaxDimValue = 1000 };

enum SignChar : uint8_t {
  kMinus = '-',
  kPlus = '+',
};

// Lines and columns are 1-based to follow the convention of most modern text
// editing software. |line| and |col| should be positive, but int16_t is used to
// limit ranges such that it matches DeltaLineCol.
struct LineCol {
  dim_t line;
  dim_t col;
};

struct DeltaLineCol {
  dim_t line;
  dim_t col;
};

constexpr DeltaLineCol operator-(const LineCol& lhs, const LineCol& rhs) {
  return DeltaLineCol{lhs.line - rhs.line, lhs.col - rhs.col};
}

constexpr LineCol operator+(const LineCol& lhs, const DeltaLineCol& rhs) {
  return LineCol{lhs.line + rhs.line, lhs.col + rhs.col};
}

}  // namespace ztf

}  // namespace zucchini

#endif  // COMPONENTS_ZUCCHINI_TYPE_ZTF_H_

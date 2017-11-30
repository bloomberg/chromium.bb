// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/cssom/CSSMathInvert.h"

#include "core/css/cssom/CSSNumericSumValue.h"

namespace blink {

WTF::Optional<CSSNumericSumValue> CSSMathInvert::SumValue() const {
  auto sum = value_->SumValue();
  if (!sum || sum->terms.size() != 1)
    return WTF::nullopt;

  for (auto& unit_exponent : sum->terms[0].units)
    unit_exponent.value *= -1;

  sum->terms[0].value = 1.0 / sum->terms[0].value;
  return sum;
}

}  // namespace blink

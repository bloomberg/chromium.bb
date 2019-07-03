// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_NUMERIC_LITERAL_VALUE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_NUMERIC_LITERAL_VALUE_H_

#include "third_party/blink/renderer/core/css/css_primitive_value.h"

namespace blink {

// Numeric values that can be expressed as a single unit (or a naked number or
// percentage). The equivalence of CSS Typed OM's |CSSUnitValue| in the
// |CSSValue| class hierarchy.
class CORE_EXPORT CSSNumericLiteralValue : public CSSPrimitiveValue {
 public:
  static CSSNumericLiteralValue* Create(double num, UnitType);

  CSSNumericLiteralValue(double num, UnitType type);

  double DoubleValue() const { return num_; }
  double ComputeSeconds() const;
  double ComputeDegrees() const;

  double ComputeLengthPx(
      const CSSToLengthConversionData& conversion_data) const;
  void AccumulateLengthArray(CSSLengthArray& length_array,
                             double multiplier) const;

  String CustomCSSText() const;
  bool Equals(const CSSNumericLiteralValue& other) const;

  void TraceAfterDispatch(blink::Visitor* visitor);

 private:
  double num_;
};

template <>
struct DowncastTraits<CSSNumericLiteralValue> {
  static bool AllowFrom(const CSSValue& value) {
    return value.IsNumericLiteralValue();
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_NUMERIC_LITERAL_VALUE_H_

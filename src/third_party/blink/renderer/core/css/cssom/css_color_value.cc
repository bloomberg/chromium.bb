// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/cssom/css_color_value.h"
#include "third_party/blink/renderer/core/css/css_color.h"
#include "third_party/blink/renderer/core/css/cssom/css_hsl.h"
#include "third_party/blink/renderer/core/css/cssom/css_rgb.h"
#include "third_party/blink/renderer/core/css/cssom/css_unit_value.h"
#include "third_party/blink/renderer/core/css/cssom/cssom_types.h"

namespace blink {

CSSRGB* CSSColorValue::toRGB() const {
  return MakeGarbageCollected<CSSRGB>(ToColor());
}

CSSHSL* CSSColorValue::toHSL() const {
  return MakeGarbageCollected<CSSHSL>(ToColor());
}

const CSSValue* CSSColorValue::ToCSSValue() const {
  return cssvalue::CSSColor::Create(ToColor().Rgb());
}

#if defined(USE_BLINK_V8_BINDING_NEW_IDL_UNION)
CSSNumericValue* CSSColorValue::ToNumberOrPercentage(
    const V8CSSNumberish* input) {
  CSSNumericValue* value = CSSNumericValue::FromPercentish(input);
  DCHECK(value);
  if (!CSSOMTypes::IsCSSStyleValueNumber(*value) &&
      !CSSOMTypes::IsCSSStyleValuePercentage(*value)) {
    return nullptr;
  }

  return value;
}
#else   // defined(USE_BLINK_V8_BINDING_NEW_IDL_UNION)
CSSNumericValue* CSSColorValue::ToNumberOrPercentage(
    const CSSNumberish& input) {
  CSSNumericValue* value = CSSNumericValue::FromPercentish(input);
  DCHECK(value);
  if (!CSSOMTypes::IsCSSStyleValueNumber(*value) &&
      !CSSOMTypes::IsCSSStyleValuePercentage(*value)) {
    return nullptr;
  }

  return value;
}
#endif  // defined(USE_BLINK_V8_BINDING_NEW_IDL_UNION)

#if defined(USE_BLINK_V8_BINDING_NEW_IDL_UNION)
CSSNumericValue* CSSColorValue::ToPercentage(const V8CSSNumberish* input) {
  CSSNumericValue* value = CSSNumericValue::FromPercentish(input);
  DCHECK(value);
  if (!CSSOMTypes::IsCSSStyleValuePercentage(*value))
    return nullptr;

  return value;
}
#else   // defined(USE_BLINK_V8_BINDING_NEW_IDL_UNION)
CSSNumericValue* CSSColorValue::ToPercentage(const CSSNumberish& input) {
  CSSNumericValue* value = CSSNumericValue::FromPercentish(input);
  DCHECK(value);
  if (!CSSOMTypes::IsCSSStyleValuePercentage(*value))
    return nullptr;

  return value;
}
#endif  // defined(USE_BLINK_V8_BINDING_NEW_IDL_UNION)

float CSSColorValue::ComponentToColorInput(CSSNumericValue* input) {
  if (CSSOMTypes::IsCSSStyleValuePercentage(*input))
    return input->to(CSSPrimitiveValue::UnitType::kPercentage)->value() / 100;
  return input->to(CSSPrimitiveValue::UnitType::kNumber)->value();
}

}  // namespace blink

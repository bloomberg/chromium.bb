// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/cssom/css_rgb.h"

#include "third_party/blink/renderer/bindings/core/v8/v8_union_cssnumericvalue_double.h"
#include "third_party/blink/renderer/core/css/cssom/css_unit_value.h"
#include "third_party/blink/renderer/platform/graphics/color.h"

namespace blink {

CSSRGB::CSSRGB(const Color& input_color) {
  double r, g, b, a;
  input_color.GetRGBA(r, g, b, a);
  r_ = CSSUnitValue::Create(r * 100, CSSPrimitiveValue::UnitType::kPercentage);
  g_ = CSSUnitValue::Create(g * 100, CSSPrimitiveValue::UnitType::kPercentage);
  b_ = CSSUnitValue::Create(b * 100, CSSPrimitiveValue::UnitType::kPercentage);
  alpha_ =
      CSSUnitValue::Create(a * 100, CSSPrimitiveValue::UnitType::kPercentage);
}

CSSRGB::CSSRGB(CSSNumericValue* r,
               CSSNumericValue* g,
               CSSNumericValue* b,
               CSSNumericValue* alpha)
    : r_(r), g_(g), b_(b), alpha_(alpha) {}

CSSRGB* CSSRGB::Create(
#if defined(USE_BLINK_V8_BINDING_NEW_IDL_UNION)
    const V8CSSNumberish* red,
    const V8CSSNumberish* green,
    const V8CSSNumberish* blue,
    const V8CSSNumberish* alpha,
#else   // defined(USE_BLINK_V8_BINDING_NEW_IDL_UNION)
    const CSSNumberish& red,
    const CSSNumberish& green,
    const CSSNumberish& blue,
    const CSSNumberish& alpha,
#endif  // defined(USE_BLINK_V8_BINDING_NEW_IDL_UNION)
    ExceptionState& exception_state) {
  CSSNumericValue* r;
  CSSNumericValue* g;
  CSSNumericValue* b;
  CSSNumericValue* a;

  if (!(r = ToNumberOrPercentage(red)) || !(g = ToNumberOrPercentage(green)) ||
      !(b = ToNumberOrPercentage(blue))) {
    exception_state.ThrowTypeError(
        "Color channel must be a number or percentage.");
    return nullptr;
  }
  if (!(a = ToPercentage(alpha))) {
    exception_state.ThrowTypeError("Alpha must be a percentage.");
    return nullptr;
  }
  return MakeGarbageCollected<CSSRGB>(r, g, b, a);
}

#if defined(USE_BLINK_V8_BINDING_NEW_IDL_UNION)

V8CSSNumberish* CSSRGB::r() const {
  return MakeGarbageCollected<V8CSSNumberish>(r_);
}

V8CSSNumberish* CSSRGB::g() const {
  return MakeGarbageCollected<V8CSSNumberish>(g_);
}

V8CSSNumberish* CSSRGB::b() const {
  return MakeGarbageCollected<V8CSSNumberish>(b_);
}

V8CSSNumberish* CSSRGB::alpha() const {
  return MakeGarbageCollected<V8CSSNumberish>(alpha_);
}

#endif  // defined(USE_BLINK_V8_BINDING_NEW_IDL_UNION)

void CSSRGB::setR(
#if defined(USE_BLINK_V8_BINDING_NEW_IDL_UNION)
    const V8CSSNumberish* red,
#else   // defined(USE_BLINK_V8_BINDING_NEW_IDL_UNION)
    const CSSNumberish& red,
#endif  // defined(USE_BLINK_V8_BINDING_NEW_IDL_UNION)
    ExceptionState& exception_state) {
  if (auto* value = ToNumberOrPercentage(red)) {
    r_ = value;
  } else {
    exception_state.ThrowTypeError(
        "Color channel must be a number or percentage.");
  }
}

void CSSRGB::setG(
#if defined(USE_BLINK_V8_BINDING_NEW_IDL_UNION)
    const V8CSSNumberish* green,
#else   // defined(USE_BLINK_V8_BINDING_NEW_IDL_UNION)
    const CSSNumberish& green,
#endif  // defined(USE_BLINK_V8_BINDING_NEW_IDL_UNION)
    ExceptionState& exception_state) {
  if (auto* value = ToNumberOrPercentage(green)) {
    g_ = value;
  } else {
    exception_state.ThrowTypeError(
        "Color channel must be a number or percentage.");
  }
}

void CSSRGB::setB(
#if defined(USE_BLINK_V8_BINDING_NEW_IDL_UNION)
    const V8CSSNumberish* blue,
#else   // defined(USE_BLINK_V8_BINDING_NEW_IDL_UNION)
    const CSSNumberish& blue,
#endif  // defined(USE_BLINK_V8_BINDING_NEW_IDL_UNION)
    ExceptionState& exception_state) {
  if (auto* value = ToNumberOrPercentage(blue)) {
    b_ = value;
  } else {
    exception_state.ThrowTypeError(
        "Color channel must be a number or percentage.");
  }
}

void CSSRGB::setAlpha(
#if defined(USE_BLINK_V8_BINDING_NEW_IDL_UNION)
    const V8CSSNumberish* alpha,
#else   // defined(USE_BLINK_V8_BINDING_NEW_IDL_UNION)
    const CSSNumberish& alpha,
#endif  // defined(USE_BLINK_V8_BINDING_NEW_IDL_UNION)
    ExceptionState& exception_state) {
  if (auto* value = ToPercentage(alpha))
    alpha_ = value;
  else
    exception_state.ThrowTypeError("Alpha must be a percentage.");
}

Color CSSRGB::ToColor() const {
  return Color(ComponentToColorInput(r_), ComponentToColorInput(g_),
               ComponentToColorInput(b_), ComponentToColorInput(alpha_));
}

}  // namespace blink

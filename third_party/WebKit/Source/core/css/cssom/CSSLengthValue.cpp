// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/cssom/CSSLengthValue.h"

#include "bindings/core/v8/ExceptionState.h"
#include "core/css/CSSPrimitiveValue.h"
#include "core/css/cssom/CSSCalcDictionary.h"
#include "core/css/cssom/CSSCalcLength.h"
#include "core/css/cssom/CSSSimpleLength.h"
#include "wtf/HashMap.h"

namespace blink {

CSSPrimitiveValue::UnitType CSSLengthValue::UnitFromName(const String& name) {
  if (EqualIgnoringASCIICase(name, "percent") || name == "%")
    return CSSPrimitiveValue::UnitType::kPercentage;
  return CSSPrimitiveValue::StringToUnitType(name);
}

CSSLengthValue* CSSLengthValue::FromCSSValue(const CSSPrimitiveValue& value) {
  if (value.IsCalculated()) {
    // TODO(meade): Implement CSSCalcLength::fromCSSValue.
    return nullptr;
  }
  return CSSSimpleLength::FromCSSValue(value);
}

CSSLengthValue* CSSLengthValue::from(const String& css_text,
                                     ExceptionState& exception_state) {
  // TODO: Implement
  return nullptr;
}

CSSLengthValue* CSSLengthValue::from(double value,
                                     const String& type,
                                     ExceptionState&) {
  return CSSSimpleLength::Create(value, UnitFromName(type));
}

CSSLengthValue* CSSLengthValue::from(const CSSCalcDictionary& dictionary,
                                     ExceptionState& exception_state) {
  return CSSCalcLength::Create(dictionary, exception_state);
}

CSSLengthValue* CSSLengthValue::add(const CSSLengthValue* other) {
  if (GetType() == kCalcLengthType)
    return AddInternal(other);

  DCHECK_EQ(GetType(), kSimpleLengthType);
  if (other->GetType() == kSimpleLengthType &&
      ToCSSSimpleLength(this)->unit() == ToCSSSimpleLength(other)->unit()) {
    return AddInternal(other);
  }

  // TODO(meade): This CalcLength is immediately thrown away. We might want
  // to optimize this at some point.
  CSSCalcLength* result = CSSCalcLength::Create(this);
  return result->add(other);
}

CSSLengthValue* CSSLengthValue::subtract(const CSSLengthValue* other) {
  if (GetType() == kCalcLengthType)
    return SubtractInternal(other);

  DCHECK_EQ(GetType(), kSimpleLengthType);
  if (other->GetType() == kSimpleLengthType &&
      ToCSSSimpleLength(this)->unit() == ToCSSSimpleLength(other)->unit()) {
    return SubtractInternal(other);
  }

  CSSCalcLength* result = CSSCalcLength::Create(this);
  return result->subtract(other);
}

CSSLengthValue* CSSLengthValue::multiply(double x) {
  return MultiplyInternal(x);
}

CSSLengthValue* CSSLengthValue::divide(double x,
                                       ExceptionState& exception_state) {
  if (x == 0) {
    exception_state.ThrowRangeError("Cannot divide by zero");
    return nullptr;
  }
  return DivideInternal(x);
}

CSSLengthValue* CSSLengthValue::AddInternal(const CSSLengthValue*) {
  NOTREACHED();
  return nullptr;
}

CSSLengthValue* CSSLengthValue::SubtractInternal(const CSSLengthValue*) {
  NOTREACHED();
  return nullptr;
}

CSSLengthValue* CSSLengthValue::MultiplyInternal(double) {
  NOTREACHED();
  return nullptr;
}

CSSLengthValue* CSSLengthValue::DivideInternal(double) {
  NOTREACHED();
  return nullptr;
}

}  // namespace blink

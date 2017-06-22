// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/cssom/CSSPerspective.h"

#include "bindings/core/v8/ExceptionState.h"

namespace blink {

CSSPerspective* CSSPerspective::Create(CSSNumericValue* length,
                                       ExceptionState& exception_state) {
  if (length->GetType() != CSSStyleValue::StyleValueType::kLengthType) {
    exception_state.ThrowTypeError("Must pass length to CSSNumericValue");
    return nullptr;
  }
  if (length->ContainsPercent()) {
    exception_state.ThrowTypeError(
        "CSSPerspective does not support CSSNumericValues with percent units");
    return nullptr;
  }
  return new CSSPerspective(length);
}

CSSPerspective* CSSPerspective::FromCSSValue(const CSSFunctionValue& value) {
  DCHECK_EQ(value.FunctionType(), CSSValuePerspective);
  DCHECK_EQ(value.length(), 1U);
  CSSNumericValue* length =
      CSSNumericValue::FromCSSValue(ToCSSPrimitiveValue(value.Item(0)));
  // TODO(meade): This shouldn't happen once CSSNumericValue is fully
  // implemented, so once that happens this check can be removed.
  if (!length)
    return nullptr;
  DCHECK(!length->ContainsPercent());
  return new CSSPerspective(length);
}

CSSFunctionValue* CSSPerspective::ToCSSValue() const {
  CSSFunctionValue* result = CSSFunctionValue::Create(CSSValuePerspective);
  result->Append(*length_->ToCSSValue());
  return result;
}

}  // namespace blink

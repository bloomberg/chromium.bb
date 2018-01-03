// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/cssom/CSSTransformComponent.h"

#include "core/css/cssom/CSSMatrixComponent.h"
#include "core/css/cssom/CSSPerspective.h"
#include "core/css/cssom/CSSRotation.h"
#include "core/css/cssom/CSSScale.h"
#include "core/css/cssom/CSSSkew.h"
#include "core/css/cssom/CSSTranslation.h"

namespace blink {

CSSTransformComponent* CSSTransformComponent::FromCSSValue(
    const CSSValue& value) {
  if (!value.IsFunctionValue())
    return nullptr;

  const CSSFunctionValue& function_value = ToCSSFunctionValue(value);
  switch (function_value.FunctionType()) {
    case CSSValueMatrix:
    case CSSValueMatrix3d:
      return CSSMatrixComponent::FromCSSValue(function_value);
    case CSSValuePerspective:
      return CSSPerspective::FromCSSValue(function_value);
    case CSSValueRotate:
    case CSSValueRotateX:
    case CSSValueRotateY:
    case CSSValueRotateZ:
    case CSSValueRotate3d:
      return CSSRotation::FromCSSValue(function_value);
    case CSSValueScale:
    case CSSValueScaleX:
    case CSSValueScaleY:
    case CSSValueScaleZ:
    case CSSValueScale3d:
      return CSSScale::FromCSSValue(function_value);
    case CSSValueSkew:
    case CSSValueSkewX:
    case CSSValueSkewY:
      return CSSSkew::FromCSSValue(function_value);
    case CSSValueTranslate:
    case CSSValueTranslateX:
    case CSSValueTranslateY:
    case CSSValueTranslateZ:
    case CSSValueTranslate3d:
      return CSSTranslation::FromCSSValue(function_value);
    default:
      return nullptr;
  }
}

String CSSTransformComponent::toString() const {
  const CSSValue* result = ToCSSValue();
  return result ? result->CssText() : "";
}

}  // namespace blink

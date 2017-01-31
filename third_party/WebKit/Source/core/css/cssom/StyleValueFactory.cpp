// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/cssom/StyleValueFactory.h"

#include "core/css/CSSImageValue.h"
#include "core/css/CSSValue.h"
#include "core/css/cssom/CSSCalcLength.h"
#include "core/css/cssom/CSSKeywordValue.h"
#include "core/css/cssom/CSSNumberValue.h"
#include "core/css/cssom/CSSOMTypes.h"
#include "core/css/cssom/CSSSimpleLength.h"
#include "core/css/cssom/CSSStyleValue.h"
#include "core/css/cssom/CSSStyleVariableReferenceValue.h"
#include "core/css/cssom/CSSTransformValue.h"
#include "core/css/cssom/CSSURLImageValue.h"
#include "core/css/cssom/CSSUnparsedValue.h"
#include "core/css/cssom/CSSUnsupportedStyleValue.h"

namespace blink {

namespace {

CSSStyleValue* createStyleValueFromPrimitiveValue(
    const CSSPrimitiveValue& primitiveValue) {
  if (primitiveValue.isNumber())
    return CSSNumberValue::create(primitiveValue.getDoubleValue());
  if (primitiveValue.isLength() || primitiveValue.isPercentage())
    return CSSSimpleLength::fromCSSValue(primitiveValue);
  return nullptr;
}

CSSStyleValue* createStyleValueWithPropertyInternal(CSSPropertyID propertyID,
                                                    const CSSValue& value) {
  switch (propertyID) {
    case CSSPropertyTransform:
      return CSSTransformValue::fromCSSValue(value);
    default:
      // TODO(meade): Implement other properties.
      break;
  }
  if (value.isPrimitiveValue() && toCSSPrimitiveValue(value).isCalculated()) {
    // TODO(meade): Handle other calculated types, e.g. angles here.
    if (CSSOMTypes::propertyCanTakeType(propertyID,
                                        CSSStyleValue::CalcLengthType)) {
      return CSSCalcLength::fromCSSValue(toCSSPrimitiveValue(value));
    }
  }
  return nullptr;
}

CSSStyleValue* createStyleValue(const CSSValue& value) {
  if (value.isCSSWideKeyword() || value.isIdentifierValue() ||
      value.isCustomIdentValue())
    return CSSKeywordValue::fromCSSValue(value);
  if (value.isPrimitiveValue())
    return createStyleValueFromPrimitiveValue(toCSSPrimitiveValue(value));
  if (value.isVariableReferenceValue())
    return CSSUnparsedValue::fromCSSValue(toCSSVariableReferenceValue(value));
  if (value.isImageValue()) {
    return CSSURLImageValue::create(
        toCSSImageValue(value).valueWithURLMadeAbsolute());
  }
  return nullptr;
}

CSSStyleValue* createStyleValueWithProperty(CSSPropertyID propertyID,
                                            const CSSValue& value) {
  CSSStyleValue* styleValue =
      createStyleValueWithPropertyInternal(propertyID, value);
  if (styleValue)
    return styleValue;
  return createStyleValue(value);
}

CSSStyleValueVector unsupportedCSSValue(const CSSValue& value) {
  CSSStyleValueVector styleValueVector;
  styleValueVector.push_back(CSSUnsupportedStyleValue::create(value.cssText()));
  return styleValueVector;
}

}  // namespace

CSSStyleValueVector StyleValueFactory::cssValueToStyleValueVector(
    CSSPropertyID propertyID,
    const CSSValue& cssValue) {
  CSSStyleValueVector styleValueVector;

  CSSStyleValue* styleValue =
      createStyleValueWithProperty(propertyID, cssValue);
  if (styleValue) {
    styleValueVector.push_back(styleValue);
    return styleValueVector;
  }

  if (!cssValue.isValueList()) {
    return unsupportedCSSValue(cssValue);
  }

  // If it's a list, we can try it as a list valued property.
  const CSSValueList& cssValueList = toCSSValueList(cssValue);
  for (const CSSValue* innerValue : cssValueList) {
    styleValue = createStyleValueWithProperty(propertyID, *innerValue);
    if (!styleValue)
      return unsupportedCSSValue(cssValue);
    styleValueVector.push_back(styleValue);
  }
  return styleValueVector;
}

CSSStyleValueVector StyleValueFactory::cssValueToStyleValueVector(
    const CSSValue& cssValue) {
  return cssValueToStyleValueVector(CSSPropertyInvalid, cssValue);
}

}  // namespace blink

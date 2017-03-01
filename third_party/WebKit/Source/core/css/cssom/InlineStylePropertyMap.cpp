// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/cssom/InlineStylePropertyMap.h"

#include "bindings/core/v8/Iterable.h"
#include "core/CSSPropertyNames.h"
#include "core/css/CSSCustomIdentValue.h"
#include "core/css/CSSCustomPropertyDeclaration.h"
#include "core/css/CSSPrimitiveValue.h"
#include "core/css/CSSPropertyMetadata.h"
#include "core/css/CSSValueList.h"
#include "core/css/StylePropertySet.h"
#include "core/css/cssom/CSSOMTypes.h"
#include "core/css/cssom/CSSSimpleLength.h"
#include "core/css/cssom/CSSUnsupportedStyleValue.h"
#include "core/css/cssom/StyleValueFactory.h"

namespace blink {

namespace {

CSSValueList* cssValueListForPropertyID(CSSPropertyID propertyID) {
  char separator = CSSPropertyMetadata::repetitionSeparator(propertyID);
  switch (separator) {
    case ' ':
      return CSSValueList::createSpaceSeparated();
    case ',':
      return CSSValueList::createCommaSeparated();
    case '/':
      return CSSValueList::createSlashSeparated();
    default:
      NOTREACHED();
      return nullptr;
  }
}

const CSSValue* styleValueToCSSValue(CSSPropertyID propertyID,
                                     const CSSStyleValue& styleValue) {
  if (!CSSOMTypes::propertyCanTake(propertyID, styleValue))
    return nullptr;
  return styleValue.toCSSValueWithProperty(propertyID);
}

const CSSValue* singleStyleValueAsCSSValue(CSSPropertyID propertyID,
                                           const CSSStyleValue& styleValue) {
  const CSSValue* cssValue = styleValueToCSSValue(propertyID, styleValue);
  if (!cssValue)
    return nullptr;

  if (!CSSPropertyMetadata::propertyIsRepeated(propertyID) ||
      cssValue->isCSSWideKeyword())
    return cssValue;

  CSSValueList* valueList = cssValueListForPropertyID(propertyID);
  valueList->append(*cssValue);
  return valueList;
}

const CSSValueList* asCSSValueList(
    CSSPropertyID propertyID,
    const CSSStyleValueVector& styleValueVector) {
  CSSValueList* valueList = cssValueListForPropertyID(propertyID);
  for (const CSSStyleValue* value : styleValueVector) {
    const CSSValue* cssValue = styleValueToCSSValue(propertyID, *value);
    if (!cssValue) {
      return nullptr;
    }
    valueList->append(*cssValue);
  }
  return valueList;
}

}  // namespace

CSSStyleValueVector InlineStylePropertyMap::getAllInternal(
    CSSPropertyID propertyID) {
  const CSSValue* cssValue =
      m_ownerElement->ensureMutableInlineStyle().getPropertyCSSValue(
          propertyID);
  if (!cssValue)
    return CSSStyleValueVector();

  return StyleValueFactory::cssValueToStyleValueVector(propertyID, *cssValue);
}

CSSStyleValueVector InlineStylePropertyMap::getAllInternal(
    AtomicString customPropertyName) {
  const CSSValue* cssValue =
      m_ownerElement->ensureMutableInlineStyle().getPropertyCSSValue(
          customPropertyName);
  if (!cssValue)
    return CSSStyleValueVector();

  return StyleValueFactory::cssValueToStyleValueVector(CSSPropertyInvalid,
                                                       *cssValue);
}

Vector<String> InlineStylePropertyMap::getProperties() {
  DEFINE_STATIC_LOCAL(const String, kAtApply, ("@apply"));
  Vector<String> result;
  bool containsAtApply = false;
  StylePropertySet& inlineStyleSet = m_ownerElement->ensureMutableInlineStyle();
  for (unsigned i = 0; i < inlineStyleSet.propertyCount(); i++) {
    CSSPropertyID propertyID = inlineStyleSet.propertyAt(i).id();
    if (propertyID == CSSPropertyVariable) {
      StylePropertySet::PropertyReference propertyReference =
          inlineStyleSet.propertyAt(i);
      const CSSCustomPropertyDeclaration& customDeclaration =
          toCSSCustomPropertyDeclaration(propertyReference.value());
      result.push_back(customDeclaration.name());
    } else if (propertyID == CSSPropertyApplyAtRule) {
      if (!containsAtApply) {
        result.push_back(kAtApply);
        containsAtApply = true;
      }
    } else {
      result.push_back(getPropertyNameString(propertyID));
    }
  }
  return result;
}

void InlineStylePropertyMap::set(
    CSSPropertyID propertyID,
    CSSStyleValueOrCSSStyleValueSequenceOrString& item,
    ExceptionState& exceptionState) {
  const CSSValue* cssValue = nullptr;
  if (item.isCSSStyleValue()) {
    cssValue =
        singleStyleValueAsCSSValue(propertyID, *item.getAsCSSStyleValue());
  } else if (item.isCSSStyleValueSequence()) {
    if (!CSSPropertyMetadata::propertyIsRepeated(propertyID)) {
      exceptionState.throwTypeError(
          "Property does not support multiple values");
      return;
    }
    cssValue = asCSSValueList(propertyID, item.getAsCSSStyleValueSequence());
  } else {
    // Parse it.
    DCHECK(item.isString());
    // TODO(meade): Implement this.
    exceptionState.throwTypeError("Not implemented yet");
    return;
  }
  if (!cssValue) {
    exceptionState.throwTypeError("Invalid type for property");
    return;
  }
  m_ownerElement->setInlineStyleProperty(propertyID, cssValue);
}

void InlineStylePropertyMap::append(
    CSSPropertyID propertyID,
    CSSStyleValueOrCSSStyleValueSequenceOrString& item,
    ExceptionState& exceptionState) {
  if (!CSSPropertyMetadata::propertyIsRepeated(propertyID)) {
    exceptionState.throwTypeError("Property does not support multiple values");
    return;
  }

  const CSSValue* cssValue =
      m_ownerElement->ensureMutableInlineStyle().getPropertyCSSValue(
          propertyID);
  CSSValueList* cssValueList = nullptr;
  if (!cssValue) {
    cssValueList = cssValueListForPropertyID(propertyID);
  } else if (cssValue->isValueList()) {
    cssValueList = toCSSValueList(cssValue)->copy();
  } else {
    // TODO(meade): Figure out what the correct behaviour here is.
    exceptionState.throwTypeError("Property is not already list valued");
    return;
  }

  if (item.isCSSStyleValue()) {
    const CSSValue* cssValue =
        styleValueToCSSValue(propertyID, *item.getAsCSSStyleValue());
    if (!cssValue) {
      exceptionState.throwTypeError("Invalid type for property");
      return;
    }
    cssValueList->append(*cssValue);
  } else if (item.isCSSStyleValueSequence()) {
    for (CSSStyleValue* styleValue : item.getAsCSSStyleValueSequence()) {
      const CSSValue* cssValue = styleValueToCSSValue(propertyID, *styleValue);
      if (!cssValue) {
        exceptionState.throwTypeError("Invalid type for property");
        return;
      }
      cssValueList->append(*cssValue);
    }
  } else {
    // Parse it.
    DCHECK(item.isString());
    // TODO(meade): Implement this.
    exceptionState.throwTypeError("Not implemented yet");
    return;
  }

  m_ownerElement->setInlineStyleProperty(propertyID, cssValueList);
}

void InlineStylePropertyMap::remove(CSSPropertyID propertyID,
                                    ExceptionState& exceptionState) {
  m_ownerElement->removeInlineStyleProperty(propertyID);
}

HeapVector<StylePropertyMap::StylePropertyMapEntry>
InlineStylePropertyMap::getIterationEntries() {
  DEFINE_STATIC_LOCAL(const String, kAtApply, ("@apply"));
  HeapVector<StylePropertyMap::StylePropertyMapEntry> result;
  StylePropertySet& inlineStyleSet = m_ownerElement->ensureMutableInlineStyle();
  for (unsigned i = 0; i < inlineStyleSet.propertyCount(); i++) {
    StylePropertySet::PropertyReference propertyReference =
        inlineStyleSet.propertyAt(i);
    CSSPropertyID propertyID = propertyReference.id();
    String name;
    CSSStyleValueOrCSSStyleValueSequence value;
    if (propertyID == CSSPropertyVariable) {
      const CSSCustomPropertyDeclaration& customDeclaration =
          toCSSCustomPropertyDeclaration(propertyReference.value());
      name = customDeclaration.name();
      // TODO(meade): Eventually custom properties will support other types, so
      // actually return them instead of always returning a
      // CSSUnsupportedStyleValue.
      value.setCSSStyleValue(
          CSSUnsupportedStyleValue::create(customDeclaration.customCSSText()));
    } else if (propertyID == CSSPropertyApplyAtRule) {
      name = kAtApply;
      value.setCSSStyleValue(CSSUnsupportedStyleValue::create(
          toCSSCustomIdentValue(propertyReference.value()).value()));
    } else {
      name = getPropertyNameString(propertyID);
      CSSStyleValueVector styleValueVector =
          StyleValueFactory::cssValueToStyleValueVector(
              propertyID, propertyReference.value());
      if (styleValueVector.size() == 1)
        value.setCSSStyleValue(styleValueVector[0]);
      else
        value.setCSSStyleValueSequence(styleValueVector);
    }
    result.push_back(std::make_pair(name, value));
  }
  return result;
}

}  // namespace blink

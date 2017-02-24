// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/cssom/CSSKeywordValue.h"

#include "bindings/core/v8/ExceptionState.h"
#include "core/css/CSSCustomIdentValue.h"
#include "core/css/CSSIdentifierValue.h"
#include "core/css/CSSInheritedValue.h"
#include "core/css/CSSInitialValue.h"
#include "core/css/CSSUnsetValue.h"
#include "core/css/parser/CSSPropertyParser.h"

namespace blink {

CSSKeywordValue* CSSKeywordValue::create(const AtomicString& keyword,
                                         ExceptionState& exceptionState) {
  if (keyword.isEmpty()) {
    exceptionState.throwTypeError(
        "CSSKeywordValue does not support empty strings");
    return nullptr;
  }
  return new CSSKeywordValue(keyword);
}

CSSKeywordValue* CSSKeywordValue::fromCSSValue(const CSSValue& value) {
  if (value.isInheritedValue())
    return new CSSKeywordValue(getValueName(CSSValueInherit));
  if (value.isInitialValue())
    return new CSSKeywordValue(getValueName(CSSValueInitial));
  if (value.isUnsetValue())
    return new CSSKeywordValue(getValueName(CSSValueUnset));
  if (value.isIdentifierValue()) {
    return new CSSKeywordValue(
        getValueName(toCSSIdentifierValue(value).getValueID()));
  }
  if (value.isCustomIdentValue()) {
    const CSSCustomIdentValue& identValue = toCSSCustomIdentValue(value);
    if (identValue.isKnownPropertyID()) {
      // CSSPropertyID represents the LHS of a CSS declaration, and
      // CSSKeywordValue represents a RHS.
      return nullptr;
    }
    return new CSSKeywordValue(identValue.value());
  }
  NOTREACHED();
  return nullptr;
}

CSSKeywordValue* CSSKeywordValue::create(const AtomicString& keyword) {
  DCHECK(!keyword.isEmpty());
  return new CSSKeywordValue(keyword);
}

const AtomicString& CSSKeywordValue::keywordValue() const {
  return m_keywordValue;
}

CSSValueID CSSKeywordValue::keywordValueID() const {
  return cssValueKeywordID(m_keywordValue);
}

CSSValue* CSSKeywordValue::toCSSValue() const {
  CSSValueID keywordID = keywordValueID();
  switch (keywordID) {
    case (CSSValueInherit):
      return CSSInheritedValue::create();
    case (CSSValueInitial):
      return CSSInitialValue::create();
    case (CSSValueUnset):
      return CSSUnsetValue::create();
    case (CSSValueInvalid):
      return CSSCustomIdentValue::create(m_keywordValue);
    default:
      return CSSIdentifierValue::create(keywordID);
  }
}

}  // namespace blink

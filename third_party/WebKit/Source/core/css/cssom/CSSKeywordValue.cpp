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
#include "platform/wtf/text/AtomicString.h"

namespace blink {

CSSKeywordValue* CSSKeywordValue::Create(const String& keyword,
                                         ExceptionState& exception_state) {
  if (keyword.IsEmpty()) {
    exception_state.ThrowTypeError(
        "CSSKeywordValue does not support empty strings");
    return nullptr;
  }
  return new CSSKeywordValue(keyword);
}

CSSKeywordValue* CSSKeywordValue::FromCSSValue(const CSSValue& value) {
  if (value.IsInheritedValue())
    return new CSSKeywordValue(getValueName(CSSValueInherit));
  if (value.IsInitialValue())
    return new CSSKeywordValue(getValueName(CSSValueInitial));
  if (value.IsUnsetValue())
    return new CSSKeywordValue(getValueName(CSSValueUnset));
  if (value.IsIdentifierValue()) {
    return new CSSKeywordValue(
        getValueName(ToCSSIdentifierValue(value).GetValueID()));
  }
  if (value.IsCustomIdentValue()) {
    const CSSCustomIdentValue& ident_value = ToCSSCustomIdentValue(value);
    if (ident_value.IsKnownPropertyID()) {
      // CSSPropertyID represents the LHS of a CSS declaration, and
      // CSSKeywordValue represents a RHS.
      return nullptr;
    }
    return new CSSKeywordValue(ident_value.Value());
  }
  NOTREACHED();
  return nullptr;
}

CSSKeywordValue* CSSKeywordValue::Create(const String& keyword) {
  DCHECK(!keyword.IsEmpty());
  return new CSSKeywordValue(keyword);
}

const String& CSSKeywordValue::value() const {
  return keyword_value_;
}

CSSValueID CSSKeywordValue::KeywordValueID() const {
  return CssValueKeywordID(keyword_value_);
}

const CSSValue* CSSKeywordValue::ToCSSValue() const {
  CSSValueID keyword_id = KeywordValueID();
  switch (keyword_id) {
    case (CSSValueInherit):
      return CSSInheritedValue::Create();
    case (CSSValueInitial):
      return CSSInitialValue::Create();
    case (CSSValueUnset):
      return CSSUnsetValue::Create();
    case (CSSValueInvalid):
      return CSSCustomIdentValue::Create(AtomicString(keyword_value_));
    default:
      return CSSIdentifierValue::Create(keyword_id);
  }
}

}  // namespace blink

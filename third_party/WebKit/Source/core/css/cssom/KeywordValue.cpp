// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/cssom/KeywordValue.h"

#include "bindings/core/v8/ExceptionState.h"
#include "core/css/CSSCustomIdentValue.h"
#include "core/css/parser/CSSParserString.h"
#include "core/css/parser/CSSPropertyParser.h"

namespace blink {

KeywordValue* KeywordValue::create(const String& keyword, ExceptionState& exceptionState)
{
    if (keyword.isEmpty()) {
        exceptionState.throwTypeError("KeywordValue does not support empty strings");
        return nullptr;
    }
    return new KeywordValue(keyword);
}

const String& KeywordValue::keywordValue() const
{
    return m_keywordValue;
}

PassRefPtrWillBeRawPtr<CSSValue> KeywordValue::toCSSValue() const
{
    CSSParserString cssKeywordString;
    cssKeywordString.init(m_keywordValue);
    CSSValueID keywordID = cssValueKeywordID(cssKeywordString);
    if (keywordID == CSSValueID::CSSValueInvalid) {
        return CSSCustomIdentValue::create(m_keywordValue);
    }
    return cssValuePool().createIdentifierValue(keywordID);
}

} // namespace blink

// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/cssom/CSSUnsupportedStyleValue.h"

#include "core/CSSPropertyNames.h"
#include "core/css/parser/CSSParser.h"

namespace blink {

CSSValue* CSSUnsupportedStyleValue::toCSSValue() const
{
    NOTREACHED();
    return nullptr;
}

CSSValue* CSSUnsupportedStyleValue::toCSSValueWithProperty(CSSPropertyID propertyID) const
{
    // TODO(sashab): Make CSSStyleValue return const CSSValue*s and remove this cast.
    return const_cast<CSSValue*>(CSSParser::parseSingleValue(propertyID, m_cssText, strictCSSParserContext()));
}

} // namespace blink

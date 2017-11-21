// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/cssom/CSSUnsupportedStyleValue.h"

#include "core/CSSPropertyNames.h"
#include "core/css/parser/CSSParser.h"

namespace blink {

const CSSValue* CSSUnsupportedStyleValue::ToCSSValue(SecureContextMode) const {
  NOTREACHED();
  return nullptr;
}

const CSSValue* CSSUnsupportedStyleValue::ToCSSValueWithProperty(
    CSSPropertyID property_id,
    SecureContextMode secure_context_mode) const {
  return CSSParser::ParseSingleValue(
      property_id, css_text_, StrictCSSParserContext(secure_context_mode));
}

}  // namespace blink

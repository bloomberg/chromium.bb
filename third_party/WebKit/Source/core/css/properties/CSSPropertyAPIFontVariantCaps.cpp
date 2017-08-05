// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/properties/CSSPropertyAPIFontVariantCaps.h"

#include "core/CSSValueKeywords.h"
#include "core/css/parser/CSSPropertyParserHelpers.h"

class CSSParserLocalContext;
namespace blink {

const CSSValue* CSSPropertyAPIFontVariantCaps::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) {
  return CSSPropertyParserHelpers::ConsumeIdent<
      CSSValueNormal, CSSValueSmallCaps, CSSValueAllSmallCaps,
      CSSValuePetiteCaps, CSSValueAllPetiteCaps, CSSValueUnicase,
      CSSValueTitlingCaps>(range);
}

}  // namespace blink

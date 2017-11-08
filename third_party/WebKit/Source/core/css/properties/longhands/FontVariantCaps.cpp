// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/properties/longhands/FontVariantCaps.h"

#include "core/CSSValueKeywords.h"
#include "core/css/parser/CSSPropertyParserHelpers.h"

namespace blink {
namespace CSSLonghand {

const CSSValue* FontVariantCaps::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return CSSPropertyParserHelpers::ConsumeIdent<
      CSSValueNormal, CSSValueSmallCaps, CSSValueAllSmallCaps,
      CSSValuePetiteCaps, CSSValueAllPetiteCaps, CSSValueUnicase,
      CSSValueTitlingCaps>(range);
}

}  // namespace CSSLonghand
}  // namespace blink

// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/properties/CSSPropertyAPIFontFeatureSettings.h"

#include "core/css/properties/CSSPropertyFontUtils.h"


namespace blink {

const CSSValue* CSSPropertyAPIFontFeatureSettings::ParseSingleValue(
    CSSPropertyID,
    CSSParserTokenRange& range,
    const CSSParserContext&,
    const CSSParserLocalContext&) const {
  return CSSPropertyFontUtils::ConsumeFontFeatureSettings(range);
}

}  // namespace blink

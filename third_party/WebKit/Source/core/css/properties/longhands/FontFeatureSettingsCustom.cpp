// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/properties/longhands/FontFeatureSettings.h"

#include "core/css/CSSFontFeatureValue.h"
#include "core/css/properties/CSSParsingUtils.h"
#include "core/style/ComputedStyle.h"

namespace blink {
namespace CSSLonghand {

const CSSValue* FontFeatureSettings::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext&,
    const CSSParserLocalContext&) const {
  return CSSParsingUtils::ConsumeFontFeatureSettings(range);
}

const CSSValue* FontFeatureSettings::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle&,
    const LayoutObject*,
    Node* styled_node,
    bool allow_visited_style) const {
  const blink::FontFeatureSettings* feature_settings =
      style.GetFontDescription().FeatureSettings();
  if (!feature_settings || !feature_settings->size())
    return CSSIdentifierValue::Create(CSSValueNormal);
  CSSValueList* list = CSSValueList::CreateCommaSeparated();
  for (unsigned i = 0; i < feature_settings->size(); ++i) {
    const FontFeature& feature = feature_settings->at(i);
    cssvalue::CSSFontFeatureValue* feature_value =
        cssvalue::CSSFontFeatureValue::Create(feature.Tag(), feature.Value());
    list->Append(*feature_value);
  }
  return list;
}

}  // namespace CSSLonghand
}  // namespace blink

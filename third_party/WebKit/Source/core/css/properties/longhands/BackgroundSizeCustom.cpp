// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/properties/longhands/background_size.h"

#include "third_party/blink/renderer/core/css/properties/computed_style_utils.h"
#include "third_party/blink/renderer/core/css/properties/css_parsing_utils.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/style/computed_style.h"

namespace blink {
namespace CSSLonghand {

const CSSValue* BackgroundSize::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext& local_context) const {
  return CSSParsingUtils::ParseBackgroundOrMaskSize(
      range, context, local_context, WebFeature::kNegativeBackgroundSize);
}

const CSSValue* BackgroundSize::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle&,
    const LayoutObject*,
    Node*,
    bool allow_visited_style) const {
  const FillLayer& fill_layer = style.BackgroundLayers();
  return ComputedStyleUtils::BackgroundImageOrWebkitMaskSize(style, fill_layer);
}

}  // namespace CSSLonghand
}  // namespace blink

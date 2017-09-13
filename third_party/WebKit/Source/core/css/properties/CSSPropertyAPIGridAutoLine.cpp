// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/properties/CSSPropertyAPIGridAutoLine.h"

#include "core/css/parser/CSSParserContext.h"
#include "core/css/properties/CSSPropertyGridUtils.h"

namespace blink {

const CSSValue* CSSPropertyAPIGridAutoLine::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return CSSPropertyGridUtils::ConsumeGridTrackList(
      range, context.Mode(), CSSPropertyGridUtils::kGridAuto);
}

}  // namespace blink

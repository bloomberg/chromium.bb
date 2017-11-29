// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/properties/longhands/GridAutoLine.h"

#include "core/css/parser/CSSParserContext.h"
#include "core/css/properties/CSSPropertyGridUtils.h"

namespace blink {
namespace CSSLonghand {

const CSSValue* GridAutoLine::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return CSSPropertyGridUtils::ConsumeGridTrackList(
      range, context.Mode(), CSSPropertyGridUtils::TrackListType::kGridAuto);
}

}  // namespace CSSLonghand
}  // namespace blink

// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/properties/CSSPropertyAPIOffsetAnchor.h"

#include "core/css/CSSValuePair.h"
#include "core/css/parser/CSSParserContext.h"
#include "core/css/parser/CSSParserTokenRange.h"
#include "core/css/parser/CSSPropertyParserHelpers.h"

class CSSParserLocalContext;
namespace blink {

using namespace CSSPropertyParserHelpers;

const CSSValue* CSSPropertyAPIOffsetAnchor::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) {
  CSSValueID id = range.Peek().Id();
  if (id == CSSValueAuto)
    return ConsumeIdent(range);
  return ConsumePosition(range, context, UnitlessQuirk::kForbid,
                         Optional<WebFeature>());
}

}  // namespace blink

// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/properties/CSSPropertyAPIClipPath.h"

#include "core/css/CSSURIValue.h"
#include "core/css/parser/CSSPropertyParserHelpers.h"
#include "core/css/properties/CSSPropertyShapeUtils.h"

class CSSParserLocalContext;
namespace blink {

const CSSValue* CSSPropertyAPIClipPath::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) {
  if (range.Peek().Id() == CSSValueNone)
    return CSSPropertyParserHelpers::ConsumeIdent(range);
  if (CSSURIValue* url = CSSPropertyParserHelpers::ConsumeUrl(range, &context))
    return url;
  return CSSPropertyShapeUtils::ConsumeBasicShape(range, context);
}

}  // namespace blink

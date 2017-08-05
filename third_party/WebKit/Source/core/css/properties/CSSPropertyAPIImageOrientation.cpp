// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/properties/CSSPropertyAPIImageOrientation.h"

#include "core/CSSValueKeywords.h"
#include "core/css/CSSPrimitiveValue.h"
#include "core/css/parser/CSSPropertyParserHelpers.h"
#include "platform/RuntimeEnabledFeatures.h"

class CSSParserLocalContext;
namespace blink {

const CSSValue* CSSPropertyAPIImageOrientation::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) {
  DCHECK(RuntimeEnabledFeatures::ImageOrientationEnabled());
  if (range.Peek().Id() == CSSValueFromImage)
    return CSSPropertyParserHelpers::ConsumeIdent(range);
  if (range.Peek().GetType() != kNumberToken) {
    CSSPrimitiveValue* angle = CSSPropertyParserHelpers::ConsumeAngle(
        range, &context, WTF::Optional<WebFeature>());
    if (angle && angle->GetDoubleValue() == 0)
      return angle;
  }
  return nullptr;
}

}  // namespace blink

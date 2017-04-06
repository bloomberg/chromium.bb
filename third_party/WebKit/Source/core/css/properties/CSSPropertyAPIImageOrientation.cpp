// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/properties/CSSPropertyAPIImageOrientation.h"

#include "core/CSSValueKeywords.h"
#include "core/css/CSSPrimitiveValue.h"
#include "core/css/parser/CSSPropertyParserHelpers.h"
#include "platform/RuntimeEnabledFeatures.h"

namespace blink {

const CSSValue* CSSPropertyAPIImageOrientation::parseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext* context) {
  DCHECK(RuntimeEnabledFeatures::imageOrientationEnabled());
  if (range.peek().id() == CSSValueFromImage)
    return CSSPropertyParserHelpers::consumeIdent(range);
  if (range.peek().type() != NumberToken) {
    CSSPrimitiveValue* angle = CSSPropertyParserHelpers::consumeAngle(range);
    if (angle && angle->getDoubleValue() == 0)
      return angle;
  }
  return nullptr;
}

}  // namespace blink

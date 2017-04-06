// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/properties/CSSPropertyAPIClip.h"

#include "core/css/CSSQuadValue.h"
#include "core/css/parser/CSSParserContext.h"
#include "core/css/parser/CSSPropertyParserHelpers.h"

namespace blink {

namespace {

CSSValue* consumeClipComponent(CSSParserTokenRange& range,
                               CSSParserMode cssParserMode) {
  if (range.peek().id() == CSSValueAuto)
    return CSSPropertyParserHelpers::consumeIdent(range);
  return CSSPropertyParserHelpers::consumeLength(
      range, cssParserMode, ValueRangeAll,
      CSSPropertyParserHelpers::UnitlessQuirk::Allow);
}

}  // namespace

const CSSValue* CSSPropertyAPIClip::parseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext* context) {
  if (range.peek().id() == CSSValueAuto)
    return CSSPropertyParserHelpers::consumeIdent(range);

  if (range.peek().functionId() != CSSValueRect)
    return nullptr;

  CSSParserTokenRange args = CSSPropertyParserHelpers::consumeFunction(range);
  // rect(t, r, b, l) || rect(t r b l)
  CSSValue* top = consumeClipComponent(args, context->mode());
  if (!top)
    return nullptr;
  bool needsComma =
      CSSPropertyParserHelpers::consumeCommaIncludingWhitespace(args);
  CSSValue* right = consumeClipComponent(args, context->mode());
  if (!right ||
      (needsComma &&
       !CSSPropertyParserHelpers::consumeCommaIncludingWhitespace(args)))
    return nullptr;
  CSSValue* bottom = consumeClipComponent(args, context->mode());
  if (!bottom ||
      (needsComma &&
       !CSSPropertyParserHelpers::consumeCommaIncludingWhitespace(args)))
    return nullptr;
  CSSValue* left = consumeClipComponent(args, context->mode());
  if (!left || !args.atEnd())
    return nullptr;
  return CSSQuadValue::create(top, right, bottom, left,
                              CSSQuadValue::SerializeAsRect);
}

}  // namespace blink

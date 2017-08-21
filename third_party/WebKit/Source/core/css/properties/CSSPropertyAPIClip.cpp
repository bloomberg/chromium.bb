// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/properties/CSSPropertyAPIClip.h"

#include "core/css/CSSQuadValue.h"
#include "core/css/parser/CSSParserContext.h"
#include "core/css/parser/CSSPropertyParserHelpers.h"

class CSSParserLocalContext;
namespace blink {

namespace {

CSSValue* ConsumeClipComponent(CSSParserTokenRange& range,
                               CSSParserMode css_parser_mode) {
  if (range.Peek().Id() == CSSValueAuto)
    return CSSPropertyParserHelpers::ConsumeIdent(range);
  return CSSPropertyParserHelpers::ConsumeLength(
      range, css_parser_mode, kValueRangeAll,
      CSSPropertyParserHelpers::UnitlessQuirk::kAllow);
}

}  // namespace

const CSSValue* CSSPropertyAPIClip::ParseSingleValue(
    CSSPropertyID,
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  if (range.Peek().Id() == CSSValueAuto)
    return CSSPropertyParserHelpers::ConsumeIdent(range);

  if (range.Peek().FunctionId() != CSSValueRect)
    return nullptr;

  CSSParserTokenRange args = CSSPropertyParserHelpers::ConsumeFunction(range);
  // rect(t, r, b, l) || rect(t r b l)
  CSSValue* top = ConsumeClipComponent(args, context.Mode());
  if (!top)
    return nullptr;
  bool needs_comma =
      CSSPropertyParserHelpers::ConsumeCommaIncludingWhitespace(args);
  CSSValue* right = ConsumeClipComponent(args, context.Mode());
  if (!right ||
      (needs_comma &&
       !CSSPropertyParserHelpers::ConsumeCommaIncludingWhitespace(args)))
    return nullptr;
  CSSValue* bottom = ConsumeClipComponent(args, context.Mode());
  if (!bottom ||
      (needs_comma &&
       !CSSPropertyParserHelpers::ConsumeCommaIncludingWhitespace(args)))
    return nullptr;
  CSSValue* left = ConsumeClipComponent(args, context.Mode());
  if (!left || !args.AtEnd())
    return nullptr;
  return CSSQuadValue::Create(top, right, bottom, left,
                              CSSQuadValue::kSerializeAsRect);
}

}  // namespace blink

// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/properties/CSSPropertyAPIScrollSnapType.h"

#include "core/css/CSSValuePair.h"
#include "core/css/parser/CSSPropertyParserHelpers.h"

namespace blink {

const CSSValue* CSSPropertyAPIScrollSnapType::parseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) {
  CSSValueID axis_id = range.Peek().Id();
  if (axis_id != CSSValueNone && axis_id != CSSValueX && axis_id != CSSValueY &&
      axis_id != CSSValueBlock && axis_id != CSSValueInline &&
      axis_id != CSSValueBoth)
    return nullptr;
  CSSValue* axis_value = CSSPropertyParserHelpers::ConsumeIdent(range);
  if (range.AtEnd() || axis_id == CSSValueNone)
    return axis_value;

  CSSValueID strictness_id = range.Peek().Id();
  if (strictness_id != CSSValueProximity && strictness_id != CSSValueMandatory)
    return axis_value;
  CSSValue* strictness_value = CSSPropertyParserHelpers::ConsumeIdent(range);
  CSSValuePair* pair = CSSValuePair::Create(axis_value, strictness_value,
                                            CSSValuePair::kDropIdenticalValues);
  return pair;
}

}  // namespace blink

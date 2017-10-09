// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/properties/CSSPropertyTransitionPropertyUtils.h"

#include "core/CSSPropertyNames.h"
#include "core/CSSValueKeywords.h"
#include "core/css/CSSCustomIdentValue.h"
#include "core/css/CSSIdentifierValue.h"
#include "core/css/CSSValue.h"
#include "core/css/CSSValueList.h"
#include "core/css/parser/CSSParserToken.h"
#include "core/css/parser/CSSParserTokenRange.h"
#include "core/css/parser/CSSPropertyParserHelpers.h"
#include "core/css/properties/CSSPropertyAPI.h"

namespace blink {

CSSValue* CSSPropertyTransitionPropertyUtils::ConsumeTransitionProperty(
    CSSParserTokenRange& range) {
  const CSSParserToken& token = range.Peek();
  if (token.GetType() != kIdentToken)
    return nullptr;
  if (token.Id() == CSSValueNone)
    return CSSPropertyParserHelpers::ConsumeIdent(range);
  CSSPropertyID unresolved_property = token.ParseAsUnresolvedCSSPropertyID();
  if (unresolved_property != CSSPropertyInvalid &&
      unresolved_property != CSSPropertyVariable) {
#if DCHECK_IS_ON()
    DCHECK(CSSPropertyAPI::Get(resolveCSSPropertyID(unresolved_property))
               .IsEnabled());
#endif
    range.ConsumeIncludingWhitespace();
    return CSSCustomIdentValue::Create(unresolved_property);
  }
  return CSSPropertyParserHelpers::ConsumeCustomIdent(range);
}

bool CSSPropertyTransitionPropertyUtils::IsValidPropertyList(
    const CSSValueList& value_list) {
  if (value_list.length() < 2)
    return true;
  for (auto& value : value_list) {
    if (value->IsIdentifierValue() &&
        ToCSSIdentifierValue(*value).GetValueID() == CSSValueNone)
      return false;
  }
  return true;
}

}  // namespace blink

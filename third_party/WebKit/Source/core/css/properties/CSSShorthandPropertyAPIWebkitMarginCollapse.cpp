// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/properties/CSSShorthandPropertyAPIWebkitMarginCollapse.h"

#include "core/css/CSSIdentifierValue.h"
#include "core/css/parser/CSSParserContext.h"
#include "core/css/parser/CSSParserFastPaths.h"
#include "core/css/parser/CSSPropertyParserHelpers.h"

namespace blink {

bool CSSShorthandPropertyAPIWebkitMarginCollapse::ParseShorthand(
    bool important,
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&,
    HeapVector<CSSProperty, 256>& properties) {
  CSSValueID id = range.ConsumeIncludingWhitespace().Id();
  if (!CSSParserFastPaths::IsValidKeywordPropertyAndValue(
          CSSPropertyWebkitMarginBeforeCollapse, id, context.Mode()))
    return false;

  CSSValue* before_collapse = CSSIdentifierValue::Create(id);
  CSSPropertyParserHelpers::AddProperty(
      CSSPropertyWebkitMarginBeforeCollapse, CSSPropertyWebkitMarginCollapse,
      *before_collapse, important,
      CSSPropertyParserHelpers::IsImplicitProperty::kNotImplicit, properties);

  if (range.AtEnd()) {
    CSSPropertyParserHelpers::AddProperty(
        CSSPropertyWebkitMarginAfterCollapse, CSSPropertyWebkitMarginCollapse,
        *before_collapse, important,
        CSSPropertyParserHelpers::IsImplicitProperty::kNotImplicit, properties);
    return true;
  }

  id = range.ConsumeIncludingWhitespace().Id();
  if (!CSSParserFastPaths::IsValidKeywordPropertyAndValue(
          CSSPropertyWebkitMarginAfterCollapse, id, context.Mode()))
    return false;
  CSSPropertyParserHelpers::AddProperty(
      CSSPropertyWebkitMarginAfterCollapse, CSSPropertyWebkitMarginCollapse,
      *CSSIdentifierValue::Create(id), important,
      CSSPropertyParserHelpers::IsImplicitProperty::kNotImplicit, properties);
  return true;
}

}  // namespace blink

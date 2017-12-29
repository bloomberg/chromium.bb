// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/properties/shorthands/Columns.h"

#include "core/css/CSSIdentifierValue.h"
#include "core/css/parser/CSSParserContext.h"
#include "core/css/parser/CSSPropertyParserHelpers.h"
#include "core/css/properties/CSSParsingUtils.h"

namespace blink {
namespace CSSShorthand {

bool Columns::ParseShorthand(
    bool important,
    CSSParserTokenRange& range,
    const CSSParserContext&,
    const CSSParserLocalContext&,
    HeapVector<CSSPropertyValue, 256>& properties) const {
  CSSValue* column_width = nullptr;
  CSSValue* column_count = nullptr;
  if (!CSSParsingUtils::ConsumeColumnWidthOrCount(range, column_width,
                                                  column_count))
    return false;
  CSSParsingUtils::ConsumeColumnWidthOrCount(range, column_width, column_count);
  if (!range.AtEnd())
    return false;
  if (!column_width)
    column_width = CSSIdentifierValue::Create(CSSValueAuto);
  if (!column_count)
    column_count = CSSIdentifierValue::Create(CSSValueAuto);
  CSSPropertyParserHelpers::AddProperty(
      CSSPropertyColumnWidth, CSSPropertyInvalid, *column_width, important,
      CSSPropertyParserHelpers::IsImplicitProperty::kNotImplicit, properties);
  CSSPropertyParserHelpers::AddProperty(
      CSSPropertyColumnCount, CSSPropertyInvalid, *column_count, important,
      CSSPropertyParserHelpers::IsImplicitProperty::kNotImplicit, properties);
  return true;
}

}  // namespace CSSShorthand
}  // namespace blink

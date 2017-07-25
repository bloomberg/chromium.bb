// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/properties/CSSPropertyGridUtils.h"

#include "core/css/parser/CSSPropertyParserHelpers.h"
#include "core/style/GridArea.h"
#include "platform/RuntimeEnabledFeatures.h"

namespace blink {

CSSCustomIdentValue* CSSPropertyGridUtils::ConsumeCustomIdentForGridLine(
    CSSParserTokenRange& range) {
  if (range.Peek().Id() == CSSValueAuto || range.Peek().Id() == CSSValueSpan ||
      range.Peek().Id() == CSSValueDefault)
    return nullptr;
  return CSSPropertyParserHelpers::ConsumeCustomIdent(range);
}

CSSValue* CSSPropertyGridUtils::ConsumeGridLine(CSSParserTokenRange& range) {
  if (range.Peek().Id() == CSSValueAuto)
    return CSSPropertyParserHelpers::ConsumeIdent(range);

  CSSIdentifierValue* span_value = nullptr;
  CSSCustomIdentValue* grid_line_name = nullptr;
  CSSPrimitiveValue* numeric_value =
      CSSPropertyParserHelpers::ConsumeInteger(range);
  if (numeric_value) {
    grid_line_name = ConsumeCustomIdentForGridLine(range);
    span_value = CSSPropertyParserHelpers::ConsumeIdent<CSSValueSpan>(range);
  } else {
    span_value = CSSPropertyParserHelpers::ConsumeIdent<CSSValueSpan>(range);
    if (span_value) {
      numeric_value = CSSPropertyParserHelpers::ConsumeInteger(range);
      grid_line_name = ConsumeCustomIdentForGridLine(range);
      if (!numeric_value)
        numeric_value = CSSPropertyParserHelpers::ConsumeInteger(range);
    } else {
      grid_line_name = ConsumeCustomIdentForGridLine(range);
      if (grid_line_name) {
        numeric_value = CSSPropertyParserHelpers::ConsumeInteger(range);
        span_value =
            CSSPropertyParserHelpers::ConsumeIdent<CSSValueSpan>(range);
        if (!span_value && !numeric_value)
          return grid_line_name;
      } else {
        return nullptr;
      }
    }
  }

  if (span_value && !numeric_value && !grid_line_name)
    return nullptr;  // "span" keyword alone is invalid.
  if (span_value && numeric_value && numeric_value->GetIntValue() < 0)
    return nullptr;  // Negative numbers are not allowed for span.
  if (numeric_value && numeric_value->GetIntValue() == 0)
    return nullptr;  // An <integer> value of zero makes the declaration
                     // invalid.

  if (numeric_value) {
    numeric_value = CSSPrimitiveValue::Create(
        clampTo(numeric_value->GetIntValue(), -kGridMaxTracks, kGridMaxTracks),
        CSSPrimitiveValue::UnitType::kInteger);
  }

  CSSValueList* values = CSSValueList::CreateSpaceSeparated();
  if (span_value)
    values->Append(*span_value);
  if (numeric_value)
    values->Append(*numeric_value);
  if (grid_line_name)
    values->Append(*grid_line_name);
  DCHECK(values->length());
  return values;
}

bool CSSPropertyGridUtils::ConsumeGridItemPositionShorthand(
    bool important,
    CSSParserTokenRange& range,
    CSSValue*& start_value,
    CSSValue*& end_value) {
  // Input should be nullptrs.
  DCHECK(!start_value);
  DCHECK(!end_value);
  DCHECK(RuntimeEnabledFeatures::CSSGridLayoutEnabled());

  start_value = ConsumeGridLine(range);
  if (!start_value)
    return false;

  if (CSSPropertyParserHelpers::ConsumeSlashIncludingWhitespace(range)) {
    end_value = ConsumeGridLine(range);
    if (!end_value)
      return false;
  } else {
    end_value = start_value->IsCustomIdentValue()
                    ? start_value
                    : CSSIdentifierValue::Create(CSSValueAuto);
  }

  return range.AtEnd();
}

}  // namespace blink

// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/properties/CSSPropertyAPIGridAutoFlow.h"

#include "core/css/CSSIdentifierValue.h"
#include "core/css/CSSValueList.h"
#include "core/css/parser/CSSPropertyParserHelpers.h"
#include "platform/RuntimeEnabledFeatures.h"

class CSSParserLocalContext;
namespace blink {

const CSSValue* CSSPropertyAPIGridAutoFlow::ParseSingleValue(
    CSSPropertyID,
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  DCHECK(RuntimeEnabledFeatures::CSSGridLayoutEnabled());
  CSSIdentifierValue* row_or_column_value =
      CSSPropertyParserHelpers::ConsumeIdent<CSSValueRow, CSSValueColumn>(
          range);
  CSSIdentifierValue* dense_algorithm =
      CSSPropertyParserHelpers::ConsumeIdent<CSSValueDense>(range);
  if (!row_or_column_value) {
    row_or_column_value =
        CSSPropertyParserHelpers::ConsumeIdent<CSSValueRow, CSSValueColumn>(
            range);
    if (!row_or_column_value && !dense_algorithm)
      return nullptr;
  }
  CSSValueList* parsed_values = CSSValueList::CreateSpaceSeparated();
  if (row_or_column_value)
    parsed_values->Append(*row_or_column_value);
  if (dense_algorithm)
    parsed_values->Append(*dense_algorithm);
  return parsed_values;
}

}  // namespace blink

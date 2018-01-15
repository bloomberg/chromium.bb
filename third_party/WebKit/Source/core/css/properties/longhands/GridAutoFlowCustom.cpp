// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/properties/longhands/GridAutoFlow.h"

#include "core/css/CSSIdentifierValue.h"
#include "core/css/CSSValueList.h"
#include "core/css/parser/CSSPropertyParserHelpers.h"
#include "core/style/ComputedStyle.h"

namespace blink {
namespace CSSLonghand {

const CSSValue* GridAutoFlow::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
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

const CSSValue* GridAutoFlow::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle&,
    const LayoutObject*,
    Node* styled_node,
    bool allow_visited_style) const {
  CSSValueList* list = CSSValueList::CreateSpaceSeparated();
  switch (style.GetGridAutoFlow()) {
    case kAutoFlowRow:
    case kAutoFlowRowDense:
      list->Append(*CSSIdentifierValue::Create(CSSValueRow));
      break;
    case kAutoFlowColumn:
    case kAutoFlowColumnDense:
      list->Append(*CSSIdentifierValue::Create(CSSValueColumn));
      break;
    default:
      NOTREACHED();
  }

  switch (style.GetGridAutoFlow()) {
    case kAutoFlowRowDense:
    case kAutoFlowColumnDense:
      list->Append(*CSSIdentifierValue::Create(CSSValueDense));
      break;
    default:
      // Do nothing.
      break;
  }

  return list;
}

}  // namespace CSSLonghand
}  // namespace blink

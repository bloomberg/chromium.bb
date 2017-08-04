// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/properties/CSSPropertyAPIGridTemplateAreas.h"

#include "core/CSSValueKeywords.h"
#include "core/css/CSSGridTemplateAreasValue.h"
#include "core/css/parser/CSSParserToken.h"
#include "core/css/parser/CSSParserTokenRange.h"
#include "core/css/parser/CSSPropertyParserHelpers.h"
#include "core/css/properties/CSSPropertyGridUtils.h"
#include "core/style/GridArea.h"
#include "platform/RuntimeEnabledFeatures.h"

namespace blink {

const CSSValue* CSSPropertyAPIGridTemplateAreas::parseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext&,
    const CSSParserLocalContext&) {
  DCHECK(RuntimeEnabledFeatures::CSSGridLayoutEnabled());
  if (range.Peek().Id() == CSSValueNone)
    return CSSPropertyParserHelpers::ConsumeIdent(range);

  NamedGridAreaMap grid_area_map;
  size_t row_count = 0;
  size_t column_count = 0;

  while (range.Peek().GetType() == kStringToken) {
    if (!CSSPropertyGridUtils::ParseGridTemplateAreasRow(
            range.ConsumeIncludingWhitespace().Value().ToString(),
            grid_area_map, row_count, column_count))
      return nullptr;
    ++row_count;
  }

  if (row_count == 0)
    return nullptr;
  DCHECK(column_count);
  return CSSGridTemplateAreasValue::Create(grid_area_map, row_count,
                                           column_count);
}

}  // namespace blink

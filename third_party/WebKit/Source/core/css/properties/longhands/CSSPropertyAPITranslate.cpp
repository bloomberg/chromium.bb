// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/properties/longhands/CSSPropertyAPITranslate.h"

#include "core/css/CSSValueList.h"
#include "core/css/parser/CSSParserTokenRange.h"
#include "core/css/parser/CSSPropertyParser.h"
#include "core/css/parser/CSSPropertyParserHelpers.h"
#include "core/layout/LayoutObject.h"

namespace blink {

const CSSValue* CSSPropertyAPITranslate::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  DCHECK(RuntimeEnabledFeatures::CSSIndependentTransformPropertiesEnabled());
  CSSValueID id = range.Peek().Id();
  if (id == CSSValueNone)
    return CSSPropertyParserHelpers::ConsumeIdent(range);

  CSSValue* translate = CSSPropertyParserHelpers::ConsumeLengthOrPercent(
      range, context.Mode(), kValueRangeAll);
  if (!translate)
    return nullptr;
  CSSValueList* list = CSSValueList::CreateSpaceSeparated();
  list->Append(*translate);
  translate = CSSPropertyParserHelpers::ConsumeLengthOrPercent(
      range, context.Mode(), kValueRangeAll);
  if (translate) {
    list->Append(*translate);
    translate = CSSPropertyParserHelpers::ConsumeLength(range, context.Mode(),
                                                        kValueRangeAll);
    if (translate)
      list->Append(*translate);
  }

  return list;
}

bool CSSPropertyAPITranslate::IsLayoutDependent(
    const ComputedStyle* style,
    LayoutObject* layout_object) const {
  return layout_object && layout_object->IsBox();
}

}  // namespace blink

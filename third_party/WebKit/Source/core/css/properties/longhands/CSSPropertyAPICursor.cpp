// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/properties/longhands/CSSPropertyAPICursor.h"

#include "core/css/CSSCursorImageValue.h"
#include "core/css/CSSValueList.h"
#include "core/css/parser/CSSParserContext.h"
#include "core/css/parser/CSSParserMode.h"
#include "core/css/parser/CSSPropertyParserHelpers.h"
#include "core/frame/UseCounter.h"

namespace blink {

const CSSValue* CSSPropertyAPICursor::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  bool in_quirks_mode = IsQuirksModeBehavior(context.Mode());
  CSSValueList* list = nullptr;
  while (CSSValue* image = CSSPropertyParserHelpers::ConsumeImage(
             range, &context,
             CSSPropertyParserHelpers::ConsumeGeneratedImagePolicy::kForbid)) {
    double num;
    IntPoint hot_spot(-1, -1);
    bool hot_spot_specified = false;
    if (CSSPropertyParserHelpers::ConsumeNumberRaw(range, num)) {
      hot_spot.SetX(int(num));
      if (!CSSPropertyParserHelpers::ConsumeNumberRaw(range, num))
        return nullptr;
      hot_spot.SetY(int(num));
      hot_spot_specified = true;
    }

    if (!list)
      list = CSSValueList::CreateCommaSeparated();

    list->Append(*cssvalue::CSSCursorImageValue::Create(
        *image, hot_spot_specified, hot_spot));
    if (!CSSPropertyParserHelpers::ConsumeCommaIncludingWhitespace(range))
      return nullptr;
  }

  CSSValueID id = range.Peek().Id();
  if (!range.AtEnd()) {
    if (id == CSSValueWebkitZoomIn)
      context.Count(WebFeature::kPrefixedCursorZoomIn);
    else if (id == CSSValueWebkitZoomOut)
      context.Count(WebFeature::kPrefixedCursorZoomOut);
  }
  CSSValue* cursor_type = nullptr;
  if (id == CSSValueHand) {
    if (!in_quirks_mode)  // Non-standard behavior
      return nullptr;
    cursor_type = CSSIdentifierValue::Create(CSSValuePointer);
    range.ConsumeIncludingWhitespace();
  } else if ((id >= CSSValueAuto && id <= CSSValueWebkitZoomOut) ||
             id == CSSValueCopy || id == CSSValueNone) {
    cursor_type = CSSPropertyParserHelpers::ConsumeIdent(range);
  } else {
    return nullptr;
  }

  if (!list)
    return cursor_type;
  list->Append(*cursor_type);
  return list;
}

}  // namespace blink

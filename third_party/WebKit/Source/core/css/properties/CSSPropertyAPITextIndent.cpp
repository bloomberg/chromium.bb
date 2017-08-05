// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/properties/CSSPropertyAPITextIndent.h"

#include "core/css/CSSValueList.h"
#include "core/css/parser/CSSParserContext.h"
#include "core/css/parser/CSSPropertyParserHelpers.h"
#include "platform/RuntimeEnabledFeatures.h"

class CSSParserLocalContext;
namespace blink {

const CSSValue* CSSPropertyAPITextIndent::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) {
  // [ <length> | <percentage> ] && hanging? && each-line?
  // Keywords only allowed when css3Text is enabled.
  CSSValueList* list = CSSValueList::CreateSpaceSeparated();

  bool has_length_or_percentage = false;
  bool has_each_line = false;
  bool has_hanging = false;

  do {
    if (!has_length_or_percentage) {
      if (CSSValue* text_indent =
              CSSPropertyParserHelpers::ConsumeLengthOrPercent(
                  range, context.Mode(), kValueRangeAll,
                  CSSPropertyParserHelpers::UnitlessQuirk::kAllow)) {
        list->Append(*text_indent);
        has_length_or_percentage = true;
        continue;
      }
    }

    if (RuntimeEnabledFeatures::CSS3TextEnabled()) {
      CSSValueID id = range.Peek().Id();
      if (!has_each_line && id == CSSValueEachLine) {
        list->Append(*CSSPropertyParserHelpers::ConsumeIdent(range));
        has_each_line = true;
        continue;
      }
      if (!has_hanging && id == CSSValueHanging) {
        list->Append(*CSSPropertyParserHelpers::ConsumeIdent(range));
        has_hanging = true;
        continue;
      }
    }
    return nullptr;
  } while (!range.AtEnd());

  if (!has_length_or_percentage)
    return nullptr;

  return list;
}

}  // namespace blink

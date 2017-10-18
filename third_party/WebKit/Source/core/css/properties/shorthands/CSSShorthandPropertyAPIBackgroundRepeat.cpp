// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/properties/shorthands/CSSShorthandPropertyAPIBackgroundRepeat.h"

#include "core/StylePropertyShorthand.h"
#include "core/css/CSSInitialValue.h"
#include "core/css/parser/CSSParserContext.h"
#include "core/css/parser/CSSPropertyParserHelpers.h"
#include "core/css/properties/CSSPropertyBackgroundUtils.h"

namespace blink {

bool CSSShorthandPropertyAPIBackgroundRepeat::ParseShorthand(
    bool important,
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext& local_context,
    HeapVector<CSSProperty, 256>& properties) const {
  CSSValue* result_x = nullptr;
  CSSValue* result_y = nullptr;
  bool implicit = false;
  if (!CSSPropertyBackgroundUtils::ConsumeRepeatStyle(range, result_x, result_y,
                                                      implicit) ||
      !range.AtEnd())
    return false;

  CSSPropertyParserHelpers::AddProperty(
      CSSPropertyBackgroundRepeatX, CSSPropertyBackgroundRepeat, *result_x,
      important,
      implicit ? CSSPropertyParserHelpers::IsImplicitProperty::kImplicit
               : CSSPropertyParserHelpers::IsImplicitProperty::kNotImplicit,
      properties);
  CSSPropertyParserHelpers::AddProperty(
      CSSPropertyBackgroundRepeatY, CSSPropertyBackgroundRepeat, *result_y,
      important,
      implicit ? CSSPropertyParserHelpers::IsImplicitProperty::kImplicit
               : CSSPropertyParserHelpers::IsImplicitProperty::kNotImplicit,
      properties);

  return true;
}

}  // namespace blink

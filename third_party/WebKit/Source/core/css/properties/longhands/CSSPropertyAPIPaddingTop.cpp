// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/properties/longhands/CSSPropertyAPIPaddingTop.h"

#include "core/css/parser/CSSParserContext.h"
#include "core/css/parser/CSSPropertyParserHelpers.h"
#include "core/layout/LayoutObject.h"
#include "core/style/ComputedStyle.h"

namespace blink {

const CSSValue* CSSPropertyAPIPaddingTop::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return ConsumeLengthOrPercent(
      range, context.Mode(), kValueRangeNonNegative,
      CSSPropertyParserHelpers::UnitlessQuirk::kAllow);
}

bool CSSPropertyAPIPaddingTop::IsLayoutDependent(
    const ComputedStyle* style,
    LayoutObject* layout_object) const {
  return layout_object && layout_object->IsBox() &&
         (!style || !style->PaddingTop().IsFixed());
}

}  // namespace blink

// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/properties/shorthands/Padding.h"

#include "core/StylePropertyShorthand.h"
#include "core/css/parser/CSSPropertyParserHelpers.h"
#include "core/layout/LayoutObject.h"
#include "core/style/ComputedStyle.h"

namespace blink {
namespace CSSShorthand {

bool Padding::ParseShorthand(
    bool important,
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&,
    HeapVector<CSSPropertyValue, 256>& properties) const {
  return CSSPropertyParserHelpers::ConsumeShorthandVia4Longhands(
      paddingShorthand(), important, context, range, properties);
}

bool Padding::IsLayoutDependent(const ComputedStyle* style,
                                LayoutObject* layout_object) const {
  return layout_object && layout_object->IsBox() &&
         (!style || !style->PaddingBottom().IsFixed() ||
          !style->PaddingTop().IsFixed() || !style->PaddingLeft().IsFixed() ||
          !style->PaddingRight().IsFixed());
}

}  // namespace CSSShorthand
}  // namespace blink

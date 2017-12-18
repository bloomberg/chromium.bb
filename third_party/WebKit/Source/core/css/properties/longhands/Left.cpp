// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/properties/longhands/Left.h"

#include "core/css/parser/CSSParserContext.h"
#include "core/css/parser/CSSPropertyParserHelpers.h"
#include "core/css/properties/CSSParsingUtils.h"
#include "core/layout/LayoutObject.h"

namespace blink {

class CSSParserLocalContext;

namespace CSSLonghand {

const CSSValue* Left::ParseSingleValue(CSSParserTokenRange& range,
                                       const CSSParserContext& context,
                                       const CSSParserLocalContext&) const {
  return CSSParsingUtils::ConsumeMarginOrOffset(
      range, context.Mode(), CSSPropertyParserHelpers::UnitlessQuirk::kAllow);
}

bool Left::IsLayoutDependent(const ComputedStyle* style,
                             LayoutObject* layout_object) const {
  return layout_object && layout_object->IsBox();
}

}  // namespace CSSLonghand
}  // namespace blink

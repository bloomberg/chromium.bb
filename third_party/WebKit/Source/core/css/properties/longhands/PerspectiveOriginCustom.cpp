// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/properties/longhands/PerspectiveOrigin.h"

#include "core/css/CSSValuePair.h"
#include "core/css/parser/CSSPropertyParserHelpers.h"
#include "core/frame/WebFeature.h"
#include "core/layout/LayoutObject.h"

namespace blink {
namespace CSSLonghand {

const CSSValue* PerspectiveOrigin::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return ConsumePosition(range, context,
                         CSSPropertyParserHelpers::UnitlessQuirk::kForbid,
                         WebFeature::kThreeValuedPositionPerspectiveOrigin);
}

bool PerspectiveOrigin::IsLayoutDependent(const ComputedStyle* style,
                                          LayoutObject* layout_object) const {
  return layout_object && layout_object->IsBox();
}

}  // namespace CSSLonghand
}  // namespace blink

// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/properties/longhands/GridTemplateLine.h"

#include "core/css/parser/CSSParserContext.h"
#include "core/css/properties/CSSPropertyGridUtils.h"
#include "core/layout/LayoutObject.h"

namespace blink {
namespace CSSLonghand {

const CSSValue* GridTemplateLine::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return CSSPropertyGridUtils::ConsumeGridTemplatesRowsOrColumns(
      range, context.Mode());
}

bool GridTemplateLine::IsLayoutDependent(const ComputedStyle* style,
                                         LayoutObject* layout_object) const {
  return layout_object && layout_object->IsLayoutGrid();
}

}  // namespace CSSLonghand
}  // namespace blink

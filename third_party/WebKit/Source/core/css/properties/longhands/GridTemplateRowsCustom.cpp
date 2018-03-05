// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/properties/longhands/GridTemplateRows.h"

#include "core/css/parser/CSSParserContext.h"
#include "core/css/properties/CSSParsingUtils.h"
#include "core/css/properties/ComputedStyleUtils.h"
#include "core/layout/LayoutObject.h"
#include "core/style/ComputedStyle.h"

namespace blink {
namespace CSSLonghand {

const CSSValue* GridTemplateRows::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return CSSParsingUtils::ConsumeGridTemplatesRowsOrColumns(range,
                                                            context.Mode());
}

bool GridTemplateRows::IsLayoutDependent(const ComputedStyle* style,
                                         LayoutObject* layout_object) const {
  return layout_object && layout_object->IsLayoutGrid();
}

const CSSValue* GridTemplateRows::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle&,
    const LayoutObject* layout_object,
    Node* styled_node,
    bool allow_visited_style) const {
  return ComputedStyleUtils::ValueForGridTrackList(kForRows, layout_object,
                                                   style);
}

}  // namespace CSSLonghand
}  // namespace blink

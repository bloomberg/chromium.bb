// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/properties/longhands/GridTemplateRows.h"

#include "core/css/parser/CSSParserContext.h"
#include "core/css/properties/CSSParsingUtils.h"
#include "core/layout/LayoutObject.h"

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

}  // namespace CSSLonghand
}  // namespace blink

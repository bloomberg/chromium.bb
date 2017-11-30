// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/properties/longhands/Transform.h"

#include "core/css/parser/CSSParserContext.h"
#include "core/css/parser/CSSParserLocalContext.h"
#include "core/css/parser/CSSParserTokenRange.h"
#include "core/css/properties/CSSParsingUtils.h"
#include "core/layout/LayoutObject.h"

namespace blink {
namespace CSSLonghand {

const CSSValue* Transform::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext& local_context) const {
  return CSSParsingUtils::ConsumeTransformList(range, context, local_context);
}

bool Transform::IsLayoutDependent(const ComputedStyle* style,
                                  LayoutObject* layout_object) const {
  return layout_object && layout_object->IsBox();
}

}  // namespace CSSLonghand
}  // namespace blink

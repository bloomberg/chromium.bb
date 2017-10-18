// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/properties/longhands/CSSPropertyAPIGridTemplateLine.h"

#include "core/css/parser/CSSParserContext.h"
#include "core/css/properties/CSSPropertyGridUtils.h"

namespace blink {

const CSSValue* CSSPropertyAPIGridTemplateLine::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return CSSPropertyGridUtils::ConsumeGridTemplatesRowsOrColumns(
      range, context.Mode());
}

}  // namespace blink

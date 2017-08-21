// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/properties/CSSPropertyAPIGridTemplateLine.h"

#include "core/css/parser/CSSParserContext.h"
#include "core/css/properties/CSSPropertyGridUtils.h"
#include "platform/RuntimeEnabledFeatures.h"

namespace blink {

const CSSValue* CSSPropertyAPIGridTemplateLine::ParseSingleValue(
    CSSPropertyID,
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  DCHECK(RuntimeEnabledFeatures::CSSGridLayoutEnabled());
  return CSSPropertyGridUtils::ConsumeGridTemplatesRowsOrColumns(
      range, context.Mode());
}

}  // namespace blink

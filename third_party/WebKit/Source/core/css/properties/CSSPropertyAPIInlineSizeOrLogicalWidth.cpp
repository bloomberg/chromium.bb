// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/properties/CSSPropertyAPIInlineSizeOrLogicalWidth.h"

#include "core/css/CSSProperty.h"
#include "core/css/parser/CSSPropertyParserHelpers.h"
#include "core/css/properties/CSSPropertyLengthUtils.h"

namespace blink {

const CSSValue* CSSPropertyAPIInlineSizeOrLogicalWidth::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return CSSPropertyLengthUtils::ConsumeWidthOrHeight(range, context);
}

const CSSPropertyAPI&
CSSPropertyAPIInlineSizeOrLogicalWidth::ResolveDirectionAwareProperty(
    TextDirection direction,
    WritingMode writing_mode) const {
  const CSSPropertyID kProperties[2] = {CSSPropertyWidth, CSSPropertyHeight};
  return ResolveToPhysicalPropertyAPI(writing_mode, kLogicalWidth, kProperties);
}
}  // namespace blink

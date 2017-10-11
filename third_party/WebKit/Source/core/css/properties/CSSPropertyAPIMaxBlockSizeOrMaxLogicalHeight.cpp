// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/properties/CSSPropertyAPIMaxBlockSizeOrMaxLogicalHeight.h"

#include "core/css/CSSProperty.h"
#include "core/css/properties/CSSPropertyLengthUtils.h"

namespace blink {

const CSSValue* CSSPropertyAPIMaxBlockSizeOrMaxLogicalHeight::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return CSSPropertyLengthUtils::ConsumeMaxWidthOrHeight(range, context);
}

const CSSPropertyAPI&
CSSPropertyAPIMaxBlockSizeOrMaxLogicalHeight::ResolveDirectionAwareProperty(
    TextDirection direction,
    WritingMode writing_mode) const {
  const CSSPropertyID kProperties[2] = {CSSPropertyMaxWidth,
                                        CSSPropertyMaxHeight};
  return ResolveToPhysicalPropertyAPI(writing_mode, kLogicalHeight,
                                      kProperties);
}
}  // namespace blink

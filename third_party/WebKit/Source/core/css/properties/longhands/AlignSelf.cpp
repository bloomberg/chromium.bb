// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/properties/longhands/AlignSelf.h"

#include "core/css/properties/CSSPropertyAlignmentUtils.h"

namespace blink {
namespace CSSLonghand {

const CSSValue* AlignSelf::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return CSSPropertyAlignmentUtils::ConsumeSelfPositionOverflowPosition(range);
}

}  // namespace CSSLonghand
}  // namespace blink

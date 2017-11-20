// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/properties/longhands/WebkitMaskBoxImageSlice.h"

#include "core/css/properties/CSSPropertyBorderImageUtils.h"

namespace blink {
namespace CSSLonghand {

const CSSValue* WebkitMaskBoxImageSlice::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext&,
    const CSSParserLocalContext&) const {
  return CSSPropertyBorderImageUtils::ConsumeBorderImageSlice(
      range, DefaultFill::kNoFill);
}

}  // namespace CSSLonghand
}  // namespace blink

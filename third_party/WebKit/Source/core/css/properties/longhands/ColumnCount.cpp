// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/properties/longhands/ColumnCount.h"

#include "core/css/properties/CSSPropertyColumnUtils.h"

namespace blink {
namespace CSSLonghand {

const CSSValue* ColumnCount::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return CSSPropertyColumnUtils::ConsumeColumnCount(range);
}

}  // namespace CSSLonghand
}  // namespace blink

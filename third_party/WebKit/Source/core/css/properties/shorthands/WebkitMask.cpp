// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/properties/shorthands/WebkitMask.h"

#include "core/css/properties/CSSParsingUtils.h"

namespace blink {
namespace CSSShorthand {

bool WebkitMask::ParseShorthand(
    bool important,
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext& local_context,
    HeapVector<CSSPropertyValue, 256>& properties) const {
  return CSSParsingUtils::ParseBackgroundOrMask(important, range, context,
                                                local_context, properties);
}

}  // namespace CSSShorthand
}  // namespace blink

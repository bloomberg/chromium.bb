// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/properties/longhands/WebkitMaskSize.h"

#include "core/css/properties/CSSParsingUtils.h"

namespace blink {
namespace CSSLonghand {

const CSSValue* WebkitMaskSize::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext& local_context) const {
  return CSSParsingUtils::ParseBackgroundOrMaskSize(range, context,
                                                    local_context);
}

}  // namespace CSSLonghand
}  // namespace blink

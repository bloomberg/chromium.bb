// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/properties/longhands/BackgroundOrigin.h"

#include "core/css/properties/CSSParsingUtils.h"

namespace blink {
namespace CSSLonghand {

const CSSValue* BackgroundOrigin::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext&,
    const CSSParserLocalContext& local_context) const {
  return CSSParsingUtils::ParseBackgroundBox(range, local_context);
}

}  // namespace CSSLonghand
}  // namespace blink

// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/properties/longhands/ImageSource.h"

#include "core/css/parser/CSSPropertyParserHelpers.h"

namespace blink {

class CSSParserLocalContext;

namespace CSSLonghand {

const CSSValue* ImageSource::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return CSSPropertyParserHelpers::ConsumeImageOrNone(range, &context);
}

}  // namespace CSSLonghand
}  // namespace blink

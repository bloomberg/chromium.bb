// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/properties/longhands/GridLine.h"

#include "core/css/properties/CSSParsingUtils.h"

namespace blink {
namespace CSSLonghand {

const CSSValue* GridLine::ParseSingleValue(CSSParserTokenRange& range,
                                           const CSSParserContext&,
                                           const CSSParserLocalContext&) const {
  return CSSParsingUtils::ConsumeGridLine(range);
}

}  // namespace CSSLonghand
}  // namespace blink

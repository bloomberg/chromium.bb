// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/properties/CSSShorthandPropertyAPIPageBreakBefore.h"

#include "core/css/parser/CSSPropertyParserHelpers.h"
#include "core/css/properties/CSSPropertyLegacyBreakUtils.h"

namespace blink {

bool CSSShorthandPropertyAPIPageBreakBefore::ParseShorthand(
    CSSPropertyID,
    bool important,
    CSSParserTokenRange& range,
    const CSSParserContext&,
    const CSSParserLocalContext&,
    HeapVector<CSSProperty, 256>& properties) const {
  CSSValueID value;
  if (!CSSPropertyLegacyBreakUtils::ConsumeFromPageBreakBetween(range, value)) {
    return false;
  }

  DCHECK_NE(value, CSSValueInvalid);
  CSSPropertyParserHelpers::AddProperty(
      CSSPropertyBreakBefore, CSSPropertyPageBreakBefore,
      *CSSIdentifierValue::Create(value), important,
      CSSPropertyParserHelpers::IsImplicitProperty::kNotImplicit, properties);
  return true;
}
}  // namespace blink

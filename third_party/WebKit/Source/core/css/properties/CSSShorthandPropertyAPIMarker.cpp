// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/properties/CSSShorthandPropertyAPIMarker.h"

#include "core/css/parser/CSSPropertyParserHelpers.h"

namespace blink {

bool CSSShorthandPropertyAPIMarker::ParseShorthand(
    CSSPropertyID,
    bool important,
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&,
    HeapVector<CSSProperty, 256>& properties) const {
  const CSSValue* marker = CSSPropertyParserHelpers::ParseLonghandViaAPI(
      CSSPropertyMarkerStart, CSSPropertyMarker, context, range);
  if (!marker || !range.AtEnd())
    return false;

  CSSPropertyParserHelpers::AddProperty(
      CSSPropertyMarkerStart, CSSPropertyMarker, *marker, important,
      CSSPropertyParserHelpers::IsImplicitProperty::kNotImplicit, properties);
  CSSPropertyParserHelpers::AddProperty(
      CSSPropertyMarkerMid, CSSPropertyMarker, *marker, important,
      CSSPropertyParserHelpers::IsImplicitProperty::kNotImplicit, properties);
  CSSPropertyParserHelpers::AddProperty(
      CSSPropertyMarkerEnd, CSSPropertyMarker, *marker, important,
      CSSPropertyParserHelpers::IsImplicitProperty::kNotImplicit, properties);
  return true;
}

}  // namespace blink

// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/properties/CSSShorthandPropertyAPIMarker.h"

#include "core/css/parser/CSSPropertyParserHelpers.h"

namespace blink {

bool CSSShorthandPropertyAPIMarker::ParseShorthand(
    bool important,
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&,
    HeapVector<CSSProperty, 256>& properties) {
  bool needs_legacy_parsing = false;
  const CSSValue* marker = CSSPropertyParserHelpers::ParseLonghandViaAPI(
      CSSPropertyMarkerStart, CSSPropertyMarker, context, range,
      needs_legacy_parsing);
  if (!marker || !range.AtEnd())
    return false;

  DCHECK(!needs_legacy_parsing);

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

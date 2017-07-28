// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/properties/CSSPropertyLegacyBreakUtils.h"

#include "core/css/CSSValueList.h"
#include "core/css/parser/CSSPropertyParserHelpers.h"

namespace blink {

namespace {

bool ConsumeCSSValueId(CSSParserTokenRange& range, CSSValueID& value) {
  CSSIdentifierValue* keyword = CSSPropertyParserHelpers::ConsumeIdent(range);
  if (!keyword || !range.AtEnd())
    return false;
  value = keyword->GetValueID();
  return true;
}

}  // namespace

bool CSSPropertyLegacyBreakUtils::ConsumeFromPageBreakBetween(
    CSSParserTokenRange& range,
    CSSValueID& value) {
  if (!ConsumeCSSValueId(range, value)) {
    return false;
  }

  if (value == CSSValueAlways) {
    value = CSSValuePage;
    return true;
  }
  return value == CSSValueAuto || value == CSSValueAvoid ||
         value == CSSValueLeft || value == CSSValueRight;
}

bool CSSPropertyLegacyBreakUtils::ConsumeFromColumnBreakBetween(
    CSSParserTokenRange& range,
    CSSValueID& value) {
  if (!ConsumeCSSValueId(range, value)) {
    return false;
  }

  if (value == CSSValueAlways) {
    value = CSSValueColumn;
    return true;
  }
  return value == CSSValueAuto || value == CSSValueAvoid;
}

bool CSSPropertyLegacyBreakUtils::ConsumeFromColumnOrPageBreakInside(
    CSSParserTokenRange& range,
    CSSValueID& value) {
  if (!ConsumeCSSValueId(range, value)) {
    return false;
  }
  return value == CSSValueAuto || value == CSSValueAvoid;
}

}  // namespace blink

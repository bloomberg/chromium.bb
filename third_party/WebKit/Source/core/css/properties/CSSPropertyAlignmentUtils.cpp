// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/properties/CSSPropertyAlignmentUtils.h"

#include "core/css/CSSValuePair.h"
#include "core/css/parser/CSSPropertyParserHelpers.h"

namespace blink {

namespace {

CSSIdentifierValue* consumeSelfPositionKeyword(CSSParserTokenRange& range) {
  CSSValueID id = range.peek().id();
  if (id == CSSValueStart || id == CSSValueEnd || id == CSSValueCenter ||
      id == CSSValueSelfStart || id == CSSValueSelfEnd ||
      id == CSSValueFlexStart || id == CSSValueFlexEnd || id == CSSValueLeft ||
      id == CSSValueRight)
    return CSSPropertyParserHelpers::consumeIdent(range);
  return nullptr;
}

CSSValue* consumeBaselineKeyword(CSSParserTokenRange& range) {
  CSSValueID id = range.peek().id();
  if (CSSPropertyParserHelpers::identMatches<CSSValueBaseline>(id))
    return CSSPropertyParserHelpers::consumeIdent(range);

  if (CSSIdentifierValue* preference =
          CSSPropertyParserHelpers::consumeIdent<CSSValueFirst, CSSValueLast>(
              range)) {
    if (range.peek().id() == CSSValueBaseline) {
      return CSSValuePair::create(preference,
                                  CSSPropertyParserHelpers::consumeIdent(range),
                                  CSSValuePair::DropIdenticalValues);
    }
  }
  return nullptr;
}

}  // namespace

CSSValue* CSSPropertyAlignmentUtils::consumeSelfPositionOverflowPosition(
    CSSParserTokenRange& range) {
  if (CSSPropertyParserHelpers::identMatches<CSSValueAuto, CSSValueNormal,
                                             CSSValueStretch>(
          range.peek().id()))
    return CSSPropertyParserHelpers::consumeIdent(range);

  if (CSSPropertyParserHelpers::identMatches<CSSValueFirst, CSSValueLast,
                                             CSSValueBaseline>(
          range.peek().id()))
    return consumeBaselineKeyword(range);

  CSSIdentifierValue* overflowPosition =
      CSSPropertyParserHelpers::consumeIdent<CSSValueUnsafe, CSSValueSafe>(
          range);
  CSSIdentifierValue* selfPosition = consumeSelfPositionKeyword(range);
  if (!selfPosition)
    return nullptr;
  if (!overflowPosition) {
    overflowPosition =
        CSSPropertyParserHelpers::consumeIdent<CSSValueUnsafe, CSSValueSafe>(
            range);
  }
  if (overflowPosition) {
    return CSSValuePair::create(selfPosition, overflowPosition,
                                CSSValuePair::DropIdenticalValues);
  }
  return selfPosition;
}

}  // namespace blink

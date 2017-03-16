// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/properties/CSSPropertyAlignmentUtils.h"

#include "core/css/CSSContentDistributionValue.h"
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

bool isContentDistributionKeyword(CSSValueID id) {
  return CSSPropertyParserHelpers::identMatches<
      CSSValueSpaceBetween, CSSValueSpaceAround, CSSValueSpaceEvenly,
      CSSValueStretch>(id);
}

bool isContentPositionKeyword(CSSValueID id) {
  return CSSPropertyParserHelpers::identMatches<
      CSSValueStart, CSSValueEnd, CSSValueCenter, CSSValueFlexStart,
      CSSValueFlexEnd, CSSValueLeft, CSSValueRight>(id);
}

bool isOverflowKeyword(CSSValueID id) {
  return CSSPropertyParserHelpers::identMatches<CSSValueUnsafe, CSSValueSafe>(
      id);
}

bool isBaselineKeyword(CSSValueID id) {
  return CSSPropertyParserHelpers::identMatches<CSSValueFirst, CSSValueLast,
                                                CSSValueBaseline>(id);
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

  if (isBaselineKeyword(range.peek().id()))
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

CSSValue* CSSPropertyAlignmentUtils::consumeContentDistributionOverflowPosition(
    CSSParserTokenRange& range) {
  CSSValueID id = range.peek().id();
  if (CSSPropertyParserHelpers::identMatches<CSSValueNormal>(id)) {
    return CSSContentDistributionValue::create(
        CSSValueInvalid, range.consumeIncludingWhitespace().id(),
        CSSValueInvalid);
  }

  if (isBaselineKeyword(id)) {
    CSSValue* baseline = consumeBaselineKeyword(range);
    if (!baseline)
      return nullptr;
    CSSValueID baselineID = CSSValueBaseline;
    if (baseline->isValuePair()) {
      if (toCSSIdentifierValue(toCSSValuePair(baseline)->first())
              .getValueID() == CSSValueLast) {
        baselineID = CSSValueLastBaseline;
      } else {
        baselineID = CSSValueFirstBaseline;
      }
    }
    return CSSContentDistributionValue::create(CSSValueInvalid, baselineID,
                                               CSSValueInvalid);
  }

  CSSValueID distribution = CSSValueInvalid;
  CSSValueID position = CSSValueInvalid;
  CSSValueID overflow = CSSValueInvalid;
  do {
    if (isContentDistributionKeyword(id)) {
      if (distribution != CSSValueInvalid)
        return nullptr;
      distribution = id;
    } else if (isContentPositionKeyword(id)) {
      if (position != CSSValueInvalid)
        return nullptr;
      position = id;
    } else if (isOverflowKeyword(id)) {
      if (overflow != CSSValueInvalid)
        return nullptr;
      overflow = id;
    } else {
      return nullptr;
    }
    range.consumeIncludingWhitespace();
    id = range.peek().id();
  } while (!range.atEnd());

  // The grammar states that we should have at least <content-distribution> or
  // <content-position>.
  if (position == CSSValueInvalid && distribution == CSSValueInvalid)
    return nullptr;

  // The grammar states that <overflow-position> must be associated to
  // <content-position>.
  if (overflow != CSSValueInvalid && position == CSSValueInvalid)
    return nullptr;

  return CSSContentDistributionValue::create(distribution, position, overflow);
}

}  // namespace blink

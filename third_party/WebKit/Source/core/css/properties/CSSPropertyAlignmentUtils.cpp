// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/properties/CSSPropertyAlignmentUtils.h"

#include "core/css/CSSContentDistributionValue.h"
#include "core/css/CSSValuePair.h"
#include "core/css/parser/CSSPropertyParserHelpers.h"

namespace blink {
namespace {

bool IsSelfPositionKeyword(CSSValueID id) {
  return CSSPropertyParserHelpers::IdentMatches<
      CSSValueStart, CSSValueEnd, CSSValueCenter, CSSValueSelfStart,
      CSSValueSelfEnd, CSSValueFlexStart, CSSValueFlexEnd, CSSValueLeft,
      CSSValueRight>(id);
}

CSSIdentifierValue* ConsumeSelfPositionKeyword(CSSParserTokenRange& range) {
  return IsSelfPositionKeyword(range.Peek().Id())
             ? CSSPropertyParserHelpers::ConsumeIdent(range)
             : nullptr;
}

bool IsAutoOrNormalOrStretch(CSSValueID id) {
  return CSSPropertyParserHelpers::IdentMatches<CSSValueAuto, CSSValueNormal,
                                                CSSValueStretch>(id);
}

bool IsContentDistributionKeyword(CSSValueID id) {
  return CSSPropertyParserHelpers::IdentMatches<
      CSSValueSpaceBetween, CSSValueSpaceAround, CSSValueSpaceEvenly,
      CSSValueStretch>(id);
}

bool IsContentPositionKeyword(CSSValueID id) {
  return CSSPropertyParserHelpers::IdentMatches<
      CSSValueStart, CSSValueEnd, CSSValueCenter, CSSValueFlexStart,
      CSSValueFlexEnd, CSSValueLeft, CSSValueRight>(id);
}

bool IsOverflowKeyword(CSSValueID id) {
  return CSSPropertyParserHelpers::IdentMatches<CSSValueUnsafe, CSSValueSafe>(
      id);
}

bool IsBaselineKeyword(CSSValueID id) {
  return CSSPropertyParserHelpers::IdentMatches<CSSValueFirst, CSSValueLast,
                                                CSSValueBaseline>(id);
}

CSSValueID GetBaselineKeyword(CSSValue& value) {
  if (!value.IsValuePair()) {
    DCHECK(ToCSSIdentifierValue(value).GetValueID() == CSSValueBaseline);
    return CSSValueBaseline;
  }

  DCHECK(ToCSSIdentifierValue(ToCSSValuePair(value).Second()).GetValueID() ==
         CSSValueBaseline);
  if (ToCSSIdentifierValue(ToCSSValuePair(value).First()).GetValueID() ==
      CSSValueLast) {
    return CSSValueLastBaseline;
  }
  DCHECK(ToCSSIdentifierValue(ToCSSValuePair(value).First()).GetValueID() ==
         CSSValueFirst);
  return CSSValueFirstBaseline;
}

CSSValue* ConsumeBaselineKeyword(CSSParserTokenRange& range) {
  CSSValueID id = range.Peek().Id();
  if (CSSPropertyParserHelpers::IdentMatches<CSSValueBaseline>(id))
    return CSSPropertyParserHelpers::ConsumeIdent(range);

  if (CSSIdentifierValue* preference =
          CSSPropertyParserHelpers::ConsumeIdent<CSSValueFirst, CSSValueLast>(
              range)) {
    if (range.Peek().Id() == CSSValueBaseline) {
      return CSSValuePair::Create(preference,
                                  CSSPropertyParserHelpers::ConsumeIdent(range),
                                  CSSValuePair::kDropIdenticalValues);
    }
  }
  return nullptr;
}

}  // namespace

CSSValue* CSSPropertyAlignmentUtils::ConsumeSelfPositionOverflowPosition(
    CSSParserTokenRange& range) {
  CSSValueID id = range.Peek().Id();
  if (IsAutoOrNormalOrStretch(id))
    return CSSPropertyParserHelpers::ConsumeIdent(range);

  if (IsBaselineKeyword(id))
    return ConsumeBaselineKeyword(range);

  CSSIdentifierValue* overflow_position =
      CSSPropertyParserHelpers::ConsumeIdent<CSSValueUnsafe, CSSValueSafe>(
          range);
  CSSIdentifierValue* self_position = ConsumeSelfPositionKeyword(range);
  if (!self_position)
    return nullptr;
  if (!overflow_position) {
    overflow_position =
        CSSPropertyParserHelpers::ConsumeIdent<CSSValueUnsafe, CSSValueSafe>(
            range);
  }
  if (overflow_position) {
    return CSSValuePair::Create(self_position, overflow_position,
                                CSSValuePair::kDropIdenticalValues);
  }
  return self_position;
}

CSSValue* CSSPropertyAlignmentUtils::ConsumeSimplifiedItemPosition(
    CSSParserTokenRange& range) {
  CSSValueID id = range.Peek().Id();
  if (IsAutoOrNormalOrStretch(id))
    return CSSPropertyParserHelpers::ConsumeIdent(range);

  if (IsBaselineKeyword(id))
    return ConsumeBaselineKeyword(range);

  return ConsumeSelfPositionKeyword(range);
}

CSSValue* CSSPropertyAlignmentUtils::ConsumeContentDistributionOverflowPosition(
    CSSParserTokenRange& range) {
  CSSValueID id = range.Peek().Id();
  if (CSSPropertyParserHelpers::IdentMatches<CSSValueNormal>(id)) {
    return CSSContentDistributionValue::Create(
        CSSValueInvalid, range.ConsumeIncludingWhitespace().Id(),
        CSSValueInvalid);
  }

  if (IsBaselineKeyword(id)) {
    CSSValue* baseline = ConsumeBaselineKeyword(range);
    if (!baseline)
      return nullptr;
    return CSSContentDistributionValue::Create(
        CSSValueInvalid, GetBaselineKeyword(*baseline), CSSValueInvalid);
  }

  CSSValueID distribution = CSSValueInvalid;
  CSSValueID position = CSSValueInvalid;
  CSSValueID overflow = CSSValueInvalid;
  do {
    if (IsContentDistributionKeyword(id)) {
      if (distribution != CSSValueInvalid)
        return nullptr;
      distribution = id;
    } else if (IsContentPositionKeyword(id)) {
      if (position != CSSValueInvalid)
        return nullptr;
      position = id;
    } else if (IsOverflowKeyword(id)) {
      if (overflow != CSSValueInvalid)
        return nullptr;
      overflow = id;
    } else {
      return nullptr;
    }
    range.ConsumeIncludingWhitespace();
    id = range.Peek().Id();
  } while (!range.AtEnd());

  // The grammar states that we should have at least <content-distribution> or
  // <content-position>.
  if (position == CSSValueInvalid && distribution == CSSValueInvalid)
    return nullptr;

  // The grammar states that <overflow-position> must be associated to
  // <content-position>.
  if (overflow != CSSValueInvalid && position == CSSValueInvalid)
    return nullptr;

  return CSSContentDistributionValue::Create(distribution, position, overflow);
}

CSSValue* CSSPropertyAlignmentUtils::ConsumeSimplifiedContentPosition(
    CSSParserTokenRange& range) {
  CSSValueID id = range.Peek().Id();
  if (CSSPropertyParserHelpers::IdentMatches<CSSValueNormal>(id) ||
      IsContentPositionKeyword(id)) {
    return CSSContentDistributionValue::Create(
        CSSValueInvalid, range.ConsumeIncludingWhitespace().Id(),
        CSSValueInvalid);
  }

  if (IsBaselineKeyword(id)) {
    CSSValue* baseline = ConsumeBaselineKeyword(range);
    if (!baseline)
      return nullptr;
    return CSSContentDistributionValue::Create(
        CSSValueInvalid, GetBaselineKeyword(*baseline), CSSValueInvalid);
  }

  if (IsContentDistributionKeyword(id)) {
    return CSSContentDistributionValue::Create(
        range.ConsumeIncludingWhitespace().Id(), CSSValueInvalid,
        CSSValueInvalid);
  }
  return nullptr;
}

}  // namespace blink

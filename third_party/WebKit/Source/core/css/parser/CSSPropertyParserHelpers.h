// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSSPropertyParserHelpers_h
#define CSSPropertyParserHelpers_h

#include "core/css/CSSCustomIdentValue.h"
#include "core/css/CSSIdentifierValue.h"
#include "core/css/CSSPrimitiveValue.h"
#include "core/css/parser/CSSParserMode.h"
#include "core/css/parser/CSSParserTokenRange.h"
#include "platform/Length.h"  // For ValueRange
#include "platform/heap/Handle.h"

namespace blink {

class CSSParserContext;
class CSSStringValue;
class CSSURIValue;
class CSSValuePair;

// When these functions are successful, they will consume all the relevant
// tokens from the range and also consume any whitespace which follows. When
// the start of the range doesn't match the type we're looking for, the range
// will not be modified.
namespace CSSPropertyParserHelpers {

void Complete4Sides(CSSValue* side[4]);

// TODO(timloh): These should probably just be consumeComma and consumeSlash.
bool ConsumeCommaIncludingWhitespace(CSSParserTokenRange&);
bool ConsumeSlashIncludingWhitespace(CSSParserTokenRange&);
// consumeFunction expects the range starts with a FunctionToken.
CSSParserTokenRange ConsumeFunction(CSSParserTokenRange&);

enum class UnitlessQuirk { kAllow, kForbid };

CSSPrimitiveValue* ConsumeInteger(
    CSSParserTokenRange&,
    double minimum_value = -std::numeric_limits<double>::max());
CSSPrimitiveValue* ConsumePositiveInteger(CSSParserTokenRange&);
bool ConsumeNumberRaw(CSSParserTokenRange&, double& result);
CSSPrimitiveValue* ConsumeNumber(CSSParserTokenRange&, ValueRange);
CSSPrimitiveValue* ConsumeLength(CSSParserTokenRange&,
                                 CSSParserMode,
                                 ValueRange,
                                 UnitlessQuirk = UnitlessQuirk::kForbid);
CSSPrimitiveValue* ConsumePercent(CSSParserTokenRange&, ValueRange);
CSSPrimitiveValue* ConsumeLengthOrPercent(
    CSSParserTokenRange&,
    CSSParserMode,
    ValueRange,
    UnitlessQuirk = UnitlessQuirk::kForbid);
CSSPrimitiveValue* ConsumeAngle(CSSParserTokenRange&);
CSSPrimitiveValue* ConsumeTime(CSSParserTokenRange&, ValueRange);
CSSPrimitiveValue* ConsumeResolution(CSSParserTokenRange&);

CSSIdentifierValue* ConsumeIdent(CSSParserTokenRange&);
CSSIdentifierValue* ConsumeIdentRange(CSSParserTokenRange&,
                                      CSSValueID lower,
                                      CSSValueID upper);
template <CSSValueID, CSSValueID...>
inline bool IdentMatches(CSSValueID id);
template <CSSValueID... allowedIdents>
CSSIdentifierValue* ConsumeIdent(CSSParserTokenRange&);

CSSCustomIdentValue* ConsumeCustomIdent(CSSParserTokenRange&);
CSSStringValue* ConsumeString(CSSParserTokenRange&);
StringView ConsumeUrlAsStringView(CSSParserTokenRange&);
CSSURIValue* ConsumeUrl(CSSParserTokenRange&, const CSSParserContext*);

CSSValue* ConsumeColor(CSSParserTokenRange&,
                       CSSParserMode,
                       bool accept_quirky_colors = false);

CSSValue* ConsumeLineWidth(CSSParserTokenRange&, CSSParserMode, UnitlessQuirk);

CSSValuePair* ConsumePosition(CSSParserTokenRange&,
                              CSSParserMode,
                              UnitlessQuirk);
bool ConsumePosition(CSSParserTokenRange&,
                     CSSParserMode,
                     UnitlessQuirk,
                     CSSValue*& result_x,
                     CSSValue*& result_y);
bool ConsumeOneOrTwoValuedPosition(CSSParserTokenRange&,
                                   CSSParserMode,
                                   UnitlessQuirk,
                                   CSSValue*& result_x,
                                   CSSValue*& result_y);

enum class ConsumeGeneratedImagePolicy { kAllow, kForbid };

CSSValue* ConsumeImage(
    CSSParserTokenRange&,
    const CSSParserContext*,
    ConsumeGeneratedImagePolicy = ConsumeGeneratedImagePolicy::kAllow);
CSSValue* ConsumeImageOrNone(CSSParserTokenRange&, const CSSParserContext*);

bool IsCSSWideKeyword(StringView);

CSSIdentifierValue* ConsumeShapeBox(CSSParserTokenRange&);

// Template implementations are at the bottom of the file for readability.

template <typename... emptyBaseCase>
inline bool IdentMatches(CSSValueID id) {
  return false;
}
template <CSSValueID head, CSSValueID... tail>
inline bool IdentMatches(CSSValueID id) {
  return id == head || IdentMatches<tail...>(id);
}

template <CSSValueID... names>
CSSIdentifierValue* ConsumeIdent(CSSParserTokenRange& range) {
  if (range.Peek().GetType() != kIdentToken ||
      !IdentMatches<names...>(range.Peek().Id()))
    return nullptr;
  return CSSIdentifierValue::Create(range.ConsumeIncludingWhitespace().Id());
}

}  // namespace CSSPropertyParserHelpers

}  // namespace blink

#endif  // CSSPropertyParserHelpers_h

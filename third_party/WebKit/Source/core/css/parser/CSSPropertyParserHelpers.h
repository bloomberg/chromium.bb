// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSSPropertyParserHelpers_h
#define CSSPropertyParserHelpers_h

#include "core/css/CSSCustomIdentValue.h"
#include "core/css/CSSIdentifierValue.h"
#include "core/css/CSSPrimitiveValue.h"
#include "core/css/CSSValueList.h"
#include "core/css/parser/CSSParserMode.h"
#include "core/css/parser/CSSParserTokenRange.h"
#include "core/frame/UseCounter.h"
#include "platform/Length.h"  // For ValueRange
#include "platform/heap/Handle.h"
#include "platform/wtf/Optional.h"

namespace blink {

class CSSParserContext;
class CSSProperty;
class CSSStringValue;
class CSSURIValue;
class CSSValuePair;
class StylePropertyShorthand;

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

CSSPrimitiveValue* ConsumeAngle(CSSParserTokenRange&,
                                const CSSParserContext&,
                                WTF::Optional<WebFeature> unitlessZeroFeature);
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
                              const CSSParserContext&,
                              UnitlessQuirk,
                              WTF::Optional<WebFeature> threeValuePosition);
bool ConsumePosition(CSSParserTokenRange&,
                     const CSSParserContext&,
                     UnitlessQuirk,
                     WTF::Optional<WebFeature> threeValuePosition,
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

enum class IsImplicitProperty { kNotImplicit, kImplicit };

void AddProperty(CSSPropertyID resolved_property,
                 CSSPropertyID current_shorthand,
                 const CSSValue&,
                 bool important,
                 IsImplicitProperty,
                 HeapVector<CSSProperty, 256>& properties);

void CountKeywordOnlyPropertyUsage(CSSPropertyID,
                                   const CSSParserContext&,
                                   CSSValueID);

const CSSValue* ParseLonghandViaAPI(CSSPropertyID unresolved_property,
                                    CSSPropertyID current_shorthand,
                                    const CSSParserContext&,
                                    CSSParserTokenRange&,
                                    bool& needs_legacy_parsing);

// ConsumeShorthandVia4LonghandsAPI and ConsumeShorthandGreedilyViaLonghandAPIs
// are based on CSSPropertyParsers' Consume4Values and ConsumeShorthandGreedily.
// They both delegate parsing of a shorthand property to its respective longhand
// components. The difference is the functions in this Helpers file expect
// component longhands to have API implementations already.
// Consume4Values and ConsumeShorthandGreedily will be deprecated soon, when
// shorthand properties are ribbonised (i.e. have their own APIs). Until then,
// there is a slight code duplication between the two versions for the following
// reasons:
// 1. An alternative to code duplicate is to have the old Consume*
//    (e.g. ConsumeShorthandGreedily) call the new Consume*
//    (e.g. ConsumeShorthandGreedilyViaLonghandAPIs). However, the
//    new Consume* expects ALL component longhands to have APIs and will parse
///   all longhands via their APIs. In order to parse shorthands, where some
//    component longhands do not have APIs, the new Consume* will need to return
//    to the old Consume* which longhands have no APIs and thus are not parsed.
//    The old Consume* will then have to parse these longhands separately.
//    Hence there's added code complexity with little code reduction.
// 2. All shorthand properties will have APIs soon, hence such code duplication
//    is temporary only.
bool ConsumeShorthandVia4LonghandsAPI(const StylePropertyShorthand&,
                                      bool important,
                                      const CSSParserContext&,
                                      CSSParserTokenRange&,
                                      HeapVector<CSSProperty, 256>& properties);

bool ConsumeShorthandGreedilyViaLonghandAPIs(
    const StylePropertyShorthand&,
    bool important,
    const CSSParserContext&,
    CSSParserTokenRange&,
    HeapVector<CSSProperty, 256>& properties);

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

// ConsumeCommaSeparatedList takes a callback function to call on each item in
// the list, followed by the arguments to pass to this callback.
// The first argument to the callback must be the CSSParserTokenRange
template <typename Func, typename... Args>
CSSValueList* ConsumeCommaSeparatedList(Func callback,
                                        CSSParserTokenRange& range,
                                        Args... args) {
  CSSValueList* list = CSSValueList::CreateCommaSeparated();
  do {
    CSSValue* value = callback(range, args...);
    if (!value)
      return nullptr;
    list->Append(*value);
  } while (ConsumeCommaIncludingWhitespace(range));
  DCHECK(list->length());
  return list;
}

CSSValue* ConsumeTransformList(CSSParserTokenRange&, const CSSParserContext&);

}  // namespace CSSPropertyParserHelpers

}  // namespace blink

#endif  // CSSPropertyParserHelpers_h

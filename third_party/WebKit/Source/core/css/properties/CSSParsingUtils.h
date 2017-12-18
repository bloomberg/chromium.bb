// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSSParsingUtils_h
#define CSSParsingUtils_h

#include "core/CSSValueKeywords.h"
#include "core/css/parser/CSSParserMode.h"
#include "core/css/parser/CSSParserTokenRange.h"
#include "core/css/parser/CSSPropertyParserHelpers.h"
#include "core/style/GridArea.h"

namespace blink {

namespace cssvalue {
class CSSFontFeatureValue;
}  // namespace cssvalue
class CSSIdentifierValue;
class CSSParserContext;
class CSSParserLocalContext;
class CSSShadowValue;
class CSSValue;
class CSSValueList;
class StylePropertyShorthand;

namespace CSSParsingUtils {

enum class AllowInsetAndSpread { kAllow, kForbid };
enum class AllowTextValue { kAllow, kForbid };
enum class DefaultFill { kFill, kNoFill };
enum class ParsingStyle { kLegacy, kNotLegacy };
enum class TrackListType { kGridTemplate, kGridTemplateNoRepeat, kGridAuto };

using ConsumeAnimationItemValue = CSSValue* (*)(CSSPropertyID,
                                                CSSParserTokenRange&,
                                                const CSSParserContext&,
                                                bool use_legacy_parsing);
using ConsumePlaceAlignmentValue = CSSValue* (*)(CSSParserTokenRange&);

constexpr size_t kMaxNumAnimationLonghands = 8;

CSSValue* ConsumeSelfPositionOverflowPosition(CSSParserTokenRange&);
CSSValue* ConsumeSimplifiedItemPosition(CSSParserTokenRange&);
CSSValue* ConsumeContentDistributionOverflowPosition(CSSParserTokenRange&);
CSSValue* ConsumeSimplifiedContentPosition(CSSParserTokenRange&);

CSSValue* ConsumeAnimationIterationCount(CSSParserTokenRange&);
CSSValue* ConsumeAnimationName(CSSParserTokenRange&,
                               const CSSParserContext&,
                               bool allow_quoted_name);
CSSValue* ConsumeAnimationTimingFunction(CSSParserTokenRange&);
bool ConsumeAnimationShorthand(
    const StylePropertyShorthand&,
    HeapVector<Member<CSSValueList>, kMaxNumAnimationLonghands>&,
    ConsumeAnimationItemValue,
    CSSParserTokenRange&,
    const CSSParserContext&,
    bool use_legacy_parsing);

void AddBackgroundValue(CSSValue*& list, CSSValue*);
CSSValue* ConsumeBackgroundAttachment(CSSParserTokenRange&);
CSSValue* ConsumeBackgroundBlendMode(CSSParserTokenRange&);
CSSValue* ConsumeBackgroundBox(CSSParserTokenRange&);
CSSValue* ConsumeBackgroundComposite(CSSParserTokenRange&);
CSSValue* ConsumeMaskSourceType(CSSParserTokenRange&);
CSSValue* ConsumeBackgroundSize(CSSParserTokenRange&,
                                CSSParserMode,
                                ParsingStyle);
bool ConsumeBackgroundPosition(CSSParserTokenRange&,
                               const CSSParserContext&,
                               CSSPropertyParserHelpers::UnitlessQuirk,
                               CSSValue*& result_x,
                               CSSValue*& result_y);
CSSValue* ConsumePrefixedBackgroundBox(CSSParserTokenRange&, AllowTextValue);

bool ConsumeRepeatStyleComponent(CSSParserTokenRange&,
                                 CSSValue*& value1,
                                 CSSValue*& value2,
                                 bool& implicit);
bool ConsumeRepeatStyle(CSSParserTokenRange&,
                        CSSValue*& result_x,
                        CSSValue*& result_y,
                        bool& implicit);

CSSValue* ConsumeWebkitBorderImage(CSSParserTokenRange&,
                                   const CSSParserContext&);
bool ConsumeBorderImageComponents(CSSParserTokenRange&,
                                  const CSSParserContext&,
                                  CSSValue*& source,
                                  CSSValue*& slice,
                                  CSSValue*& width,
                                  CSSValue*& outset,
                                  CSSValue*& repeat,
                                  DefaultFill);
CSSValue* ConsumeBorderImageRepeat(CSSParserTokenRange&);
CSSValue* ConsumeBorderImageSlice(CSSParserTokenRange&, DefaultFill);
CSSValue* ConsumeBorderImageWidth(CSSParserTokenRange&);
CSSValue* ConsumeBorderImageOutset(CSSParserTokenRange&);

CSSValue* ConsumeShadow(CSSParserTokenRange&,
                        CSSParserMode,
                        AllowInsetAndSpread);
CSSShadowValue* ParseSingleShadow(CSSParserTokenRange&,
                                  CSSParserMode,
                                  AllowInsetAndSpread);

CSSValue* ConsumeColumnCount(CSSParserTokenRange&);
CSSValue* ConsumeColumnWidth(CSSParserTokenRange&);
bool ConsumeColumnWidthOrCount(CSSParserTokenRange&, CSSValue*&, CSSValue*&);

CSSValue* ConsumeCounter(CSSParserTokenRange&, int);

CSSValue* ConsumeFontSize(CSSParserTokenRange&,
                          CSSParserMode,
                          CSSPropertyParserHelpers::UnitlessQuirk =
                              CSSPropertyParserHelpers::UnitlessQuirk::kForbid);

CSSValue* ConsumeLineHeight(CSSParserTokenRange&, CSSParserMode);

CSSValueList* ConsumeFontFamily(CSSParserTokenRange&);
CSSValue* ConsumeGenericFamily(CSSParserTokenRange&);
CSSValue* ConsumeFamilyName(CSSParserTokenRange&);
String ConcatenateFamilyName(CSSParserTokenRange&);
CSSIdentifierValue* ConsumeFontStretchKeywordOnly(CSSParserTokenRange&);
CSSValue* ConsumeFontStretch(CSSParserTokenRange&, const CSSParserMode&);
CSSValue* ConsumeFontStyle(CSSParserTokenRange&, const CSSParserMode&);
CSSValue* ConsumeFontWeight(CSSParserTokenRange&, const CSSParserMode&);
CSSValue* ConsumeFontFeatureSettings(CSSParserTokenRange&);
cssvalue::CSSFontFeatureValue* ConsumeFontFeatureTag(CSSParserTokenRange&);
CSSIdentifierValue* ConsumeFontVariantCSS21(CSSParserTokenRange&);

CSSValue* ConsumeGridLine(CSSParserTokenRange&);
CSSValue* ConsumeGridTrackList(CSSParserTokenRange&,
                               CSSParserMode,
                               TrackListType);
bool ParseGridTemplateAreasRow(const WTF::String& grid_row_names,
                               NamedGridAreaMap&,
                               const size_t row_count,
                               size_t& column_count);
CSSValue* ConsumeGridTemplatesRowsOrColumns(CSSParserTokenRange&,
                                            CSSParserMode);
bool ConsumeGridItemPositionShorthand(bool important,
                                      CSSParserTokenRange&,
                                      CSSValue*& start_value,
                                      CSSValue*& end_value);
bool ConsumeGridTemplateShorthand(bool important,
                                  CSSParserTokenRange&,
                                  const CSSParserContext&,
                                  CSSValue*& template_rows,
                                  CSSValue*& template_columns,
                                  CSSValue*& template_areas);

// The fragmentation spec says that page-break-(after|before|inside) are to be
// treated as shorthands for their break-(after|before|inside) counterparts.
// We'll do the same for the non-standard properties
// -webkit-column-break-(after|before|inside).
bool ConsumeFromPageBreakBetween(CSSParserTokenRange&, CSSValueID&);
bool ConsumeFromColumnBreakBetween(CSSParserTokenRange&, CSSValueID&);
bool ConsumeFromColumnOrPageBreakInside(CSSParserTokenRange&, CSSValueID&);

CSSValue* ConsumeMaxWidthOrHeight(
    CSSParserTokenRange&,
    const CSSParserContext&,
    CSSPropertyParserHelpers::UnitlessQuirk =
        CSSPropertyParserHelpers::UnitlessQuirk::kForbid);
CSSValue* ConsumeWidthOrHeight(
    CSSParserTokenRange&,
    const CSSParserContext&,
    CSSPropertyParserHelpers::UnitlessQuirk =
        CSSPropertyParserHelpers::UnitlessQuirk::kForbid);

CSSValue* ConsumeMarginOrOffset(CSSParserTokenRange&,
                                CSSParserMode,
                                CSSPropertyParserHelpers::UnitlessQuirk);
CSSValue* ConsumeOffsetPath(CSSParserTokenRange&, const CSSParserContext&);
CSSValue* ConsumePathOrNone(CSSParserTokenRange&);
CSSValue* ConsumeOffsetRotate(CSSParserTokenRange&, const CSSParserContext&);

bool ConsumePlaceAlignment(CSSParserTokenRange&,
                           ConsumePlaceAlignmentValue,
                           CSSValue*& align_value,
                           CSSValue*& justify_value);

CSSValue* ConsumeBasicShape(CSSParserTokenRange&, const CSSParserContext&);
bool ConsumeRadii(CSSValue* horizontal_radii[4],
                  CSSValue* vertical_radii[4],
                  CSSParserTokenRange&,
                  CSSParserMode,
                  bool use_legacy_parsing);

CSSValue* ConsumeTextDecorationLine(CSSParserTokenRange&);

CSSValue* ConsumeTransformList(CSSParserTokenRange&,
                               const CSSParserContext&,
                               const CSSParserLocalContext&);
CSSValue* ConsumeTransitionProperty(CSSParserTokenRange&);
bool IsValidPropertyList(const CSSValueList&);

CSSValue* ConsumeBorderColorSide(CSSParserTokenRange&,
                                 const CSSParserContext&,
                                 const CSSParserLocalContext&);
CSSValue* ConsumeBorderWidth(CSSParserTokenRange&,
                             CSSParserMode,
                             CSSPropertyParserHelpers::UnitlessQuirk);

template <CSSValueID start, CSSValueID end>
CSSValue* ConsumePositionLonghand(CSSParserTokenRange& range,
                                  CSSParserMode css_parser_mode) {
  if (range.Peek().GetType() == kIdentToken) {
    CSSValueID id = range.Peek().Id();
    int percent;
    if (id == start)
      percent = 0;
    else if (id == CSSValueCenter)
      percent = 50;
    else if (id == end)
      percent = 100;
    else
      return nullptr;
    range.ConsumeIncludingWhitespace();
    return CSSPrimitiveValue::Create(percent,
                                     CSSPrimitiveValue::UnitType::kPercentage);
  }
  return CSSPropertyParserHelpers::ConsumeLengthOrPercent(
      range, css_parser_mode, kValueRangeAll);
}

}  // namespace CSSParsingUtils
}  // namespace blink

#endif  // CSSParsingUtils_h

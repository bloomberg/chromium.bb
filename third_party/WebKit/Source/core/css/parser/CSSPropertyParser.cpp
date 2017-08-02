// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/parser/CSSPropertyParser.h"

#include <memory>
#include "core/StylePropertyShorthand.h"
#include "core/css/CSSBasicShapeValues.h"
#include "core/css/CSSContentDistributionValue.h"
#include "core/css/CSSCursorImageValue.h"
#include "core/css/CSSFontFaceSrcValue.h"
#include "core/css/CSSFunctionValue.h"
#include "core/css/CSSGridAutoRepeatValue.h"
#include "core/css/CSSGridLineNamesValue.h"
#include "core/css/CSSGridTemplateAreasValue.h"
#include "core/css/CSSIdentifierValue.h"
#include "core/css/CSSInheritedValue.h"
#include "core/css/CSSInitialValue.h"
#include "core/css/CSSPendingSubstitutionValue.h"
#include "core/css/CSSPrimitiveValueMappings.h"
#include "core/css/CSSReflectValue.h"
#include "core/css/CSSShadowValue.h"
#include "core/css/CSSStringValue.h"
#include "core/css/CSSURIValue.h"
#include "core/css/CSSUnicodeRangeValue.h"
#include "core/css/CSSUnsetValue.h"
#include "core/css/CSSValuePair.h"
#include "core/css/CSSVariableReferenceValue.h"
#include "core/css/HashTools.h"
#include "core/css/parser/CSSParserFastPaths.h"
#include "core/css/parser/CSSParserIdioms.h"
#include "core/css/parser/CSSParserLocalContext.h"
#include "core/css/parser/CSSPropertyParserHelpers.h"
#include "core/css/parser/CSSVariableParser.h"
#include "core/css/properties/CSSPropertyAlignmentUtils.h"
#include "core/css/properties/CSSPropertyAnimationTimingFunctionUtils.h"
#include "core/css/properties/CSSPropertyBackgroundUtils.h"
#include "core/css/properties/CSSPropertyBorderImageUtils.h"
#include "core/css/properties/CSSPropertyDescriptor.h"
#include "core/css/properties/CSSPropertyFontUtils.h"
#include "core/css/properties/CSSPropertyGridTemplateAreasUtils.h"
#include "core/css/properties/CSSPropertyGridUtils.h"
#include "core/css/properties/CSSPropertyLengthUtils.h"
#include "core/css/properties/CSSPropertyMarginUtils.h"
#include "core/css/properties/CSSPropertyPositionUtils.h"
#include "core/css/properties/CSSPropertyTextDecorationLineUtils.h"
#include "core/css/properties/CSSPropertyTransitionPropertyUtils.h"
#include "core/frame/UseCounter.h"
#include "core/layout/LayoutTheme.h"
#include "platform/wtf/text/StringBuilder.h"

namespace blink {

using namespace CSSPropertyParserHelpers;

class CSSCustomIdentValue;

CSSPropertyParser::CSSPropertyParser(
    const CSSParserTokenRange& range,
    const CSSParserContext* context,
    HeapVector<CSSProperty, 256>* parsed_properties)
    : range_(range), context_(context), parsed_properties_(parsed_properties) {
  range_.ConsumeWhitespace();
}

// AddProperty takes implicit as an enum, below we're using a bool because
// AddParsedProperty will be removed after we finish implemenation of property
// APIs.
void CSSPropertyParser::AddParsedProperty(CSSPropertyID resolved_property,
                                          CSSPropertyID current_shorthand,
                                          const CSSValue& value,
                                          bool important,
                                          bool implicit) {
  AddProperty(resolved_property, current_shorthand, value, important,
              implicit ? IsImplicitProperty::kImplicit
                       : IsImplicitProperty::kNotImplicit,
              *parsed_properties_);
}

void CSSPropertyParser::AddExpandedPropertyForValue(CSSPropertyID property,
                                                    const CSSValue& value,
                                                    bool important) {
  const StylePropertyShorthand& shorthand = shorthandForProperty(property);
  unsigned shorthand_length = shorthand.length();
  DCHECK(shorthand_length);
  const CSSPropertyID* longhands = shorthand.properties();
  for (unsigned i = 0; i < shorthand_length; ++i)
    AddParsedProperty(longhands[i], property, value, important);
}

bool CSSPropertyParser::ParseValue(
    CSSPropertyID unresolved_property,
    bool important,
    const CSSParserTokenRange& range,
    const CSSParserContext* context,
    HeapVector<CSSProperty, 256>& parsed_properties,
    StyleRule::RuleType rule_type) {
  int parsed_properties_size = parsed_properties.size();

  CSSPropertyParser parser(range, context, &parsed_properties);
  CSSPropertyID resolved_property = resolveCSSPropertyID(unresolved_property);
  bool parse_success;

  if (rule_type == StyleRule::kViewport) {
    parse_success =
        (RuntimeEnabledFeatures::CSSViewportEnabled() ||
         IsUASheetBehavior(context->Mode())) &&
        parser.ParseViewportDescriptor(resolved_property, important);
  } else if (rule_type == StyleRule::kFontFace) {
    parse_success = parser.ParseFontFaceDescriptor(resolved_property);
  } else {
    parse_success = parser.ParseValueStart(unresolved_property, important);
  }

  // This doesn't count UA style sheets
  if (parse_success)
    context->Count(context->Mode(), unresolved_property);

  if (!parse_success)
    parsed_properties.Shrink(parsed_properties_size);

  return parse_success;
}

const CSSValue* CSSPropertyParser::ParseSingleValue(
    CSSPropertyID property,
    const CSSParserTokenRange& range,
    const CSSParserContext* context) {
  CSSPropertyParser parser(range, context, nullptr);
  const CSSValue* value = parser.ParseSingleValue(property);
  if (!value || !parser.range_.AtEnd())
    return nullptr;
  return value;
}

bool CSSPropertyParser::ParseValueStart(CSSPropertyID unresolved_property,
                                        bool important) {
  if (ConsumeCSSWideKeyword(unresolved_property, important))
    return true;

  CSSParserTokenRange original_range = range_;
  CSSPropertyID property_id = resolveCSSPropertyID(unresolved_property);
  bool is_shorthand = isShorthandProperty(property_id);

  if (is_shorthand) {
    // Variable references will fail to parse here and will fall out to the
    // variable ref parser below.
    if (ParseShorthand(unresolved_property, important))
      return true;
  } else {
    if (const CSSValue* parsed_value = ParseSingleValue(unresolved_property)) {
      if (range_.AtEnd()) {
        AddParsedProperty(property_id, CSSPropertyInvalid, *parsed_value,
                          important);
        return true;
      }
    }
  }

  if (CSSVariableParser::ContainsValidVariableReferences(original_range)) {
    bool is_animation_tainted = false;
    CSSVariableReferenceValue* variable = CSSVariableReferenceValue::Create(
        CSSVariableData::Create(original_range, is_animation_tainted, true),
        *context_);

    if (is_shorthand) {
      const CSSPendingSubstitutionValue& pending_value =
          *CSSPendingSubstitutionValue::Create(property_id, variable);
      AddExpandedPropertyForValue(property_id, pending_value, important);
    } else {
      AddParsedProperty(property_id, CSSPropertyInvalid, *variable, important);
    }
    return true;
  }

  return false;
}

template <typename CharacterType>
static CSSPropertyID UnresolvedCSSPropertyID(const CharacterType* property_name,
                                             unsigned length) {
  if (length == 0)
    return CSSPropertyInvalid;
  if (length >= 2 && property_name[0] == '-' && property_name[1] == '-')
    return CSSPropertyVariable;
  if (length > maxCSSPropertyNameLength)
    return CSSPropertyInvalid;

  char buffer[maxCSSPropertyNameLength + 1];  // 1 for null character

  for (unsigned i = 0; i != length; ++i) {
    CharacterType c = property_name[i];
    if (c == 0 || c >= 0x7F)
      return CSSPropertyInvalid;  // illegal character
    buffer[i] = ToASCIILower(c);
  }
  buffer[length] = '\0';

  const char* name = buffer;
  const Property* hash_table_entry = FindProperty(name, length);
  if (!hash_table_entry)
    return CSSPropertyInvalid;
  CSSPropertyID property = static_cast<CSSPropertyID>(hash_table_entry->id);
  if (!CSSPropertyMetadata::IsEnabledProperty(property))
    return CSSPropertyInvalid;
  return property;
}

CSSPropertyID unresolvedCSSPropertyID(const String& string) {
  unsigned length = string.length();
  return string.Is8Bit()
             ? UnresolvedCSSPropertyID(string.Characters8(), length)
             : UnresolvedCSSPropertyID(string.Characters16(), length);
}

CSSPropertyID UnresolvedCSSPropertyID(StringView string) {
  unsigned length = string.length();
  return string.Is8Bit()
             ? UnresolvedCSSPropertyID(string.Characters8(), length)
             : UnresolvedCSSPropertyID(string.Characters16(), length);
}

template <typename CharacterType>
static CSSValueID CssValueKeywordID(const CharacterType* value_keyword,
                                    unsigned length) {
  char buffer[maxCSSValueKeywordLength + 1];  // 1 for null character

  for (unsigned i = 0; i != length; ++i) {
    CharacterType c = value_keyword[i];
    if (c == 0 || c >= 0x7F)
      return CSSValueInvalid;  // illegal character
    buffer[i] = WTF::ToASCIILower(c);
  }
  buffer[length] = '\0';

  const Value* hash_table_entry = FindValue(buffer, length);
  return hash_table_entry ? static_cast<CSSValueID>(hash_table_entry->id)
                          : CSSValueInvalid;
}

CSSValueID CssValueKeywordID(StringView string) {
  unsigned length = string.length();
  if (!length)
    return CSSValueInvalid;
  if (length > maxCSSValueKeywordLength)
    return CSSValueInvalid;

  return string.Is8Bit() ? CssValueKeywordID(string.Characters8(), length)
                         : CssValueKeywordID(string.Characters16(), length);
}

bool CSSPropertyParser::ConsumeCSSWideKeyword(CSSPropertyID unresolved_property,
                                              bool important) {
  CSSParserTokenRange range_copy = range_;
  CSSValueID id = range_copy.ConsumeIncludingWhitespace().Id();
  if (!range_copy.AtEnd())
    return false;

  CSSValue* value = nullptr;
  if (id == CSSValueInitial)
    value = CSSInitialValue::Create();
  else if (id == CSSValueInherit)
    value = CSSInheritedValue::Create();
  else if (id == CSSValueUnset)
    value = CSSUnsetValue::Create();
  else
    return false;

  CSSPropertyID property = resolveCSSPropertyID(unresolved_property);
  const StylePropertyShorthand& shorthand = shorthandForProperty(property);
  if (!shorthand.length()) {
    if (!CSSPropertyMetadata::IsProperty(unresolved_property))
      return false;
    AddParsedProperty(property, CSSPropertyInvalid, *value, important);
  } else {
    AddExpandedPropertyForValue(property, *value, important);
  }
  range_ = range_copy;
  return true;
}

static CSSValue* ConsumeFontVariantList(CSSParserTokenRange& range) {
  CSSValueList* values = CSSValueList::CreateCommaSeparated();
  do {
    if (range.Peek().Id() == CSSValueAll) {
      // FIXME: CSSPropertyParser::ParseFontVariant() implements
      // the old css3 draft:
      // http://www.w3.org/TR/2002/WD-css3-webfonts-20020802/#font-variant
      // 'all' is only allowed in @font-face and with no other values.
      if (values->length())
        return nullptr;
      return ConsumeIdent(range);
    }
    CSSIdentifierValue* font_variant =
        CSSPropertyFontUtils::ConsumeFontVariantCSS21(range);
    if (font_variant)
      values->Append(*font_variant);
  } while (ConsumeCommaIncludingWhitespace(range));

  if (values->length())
    return values;

  return nullptr;
}

static CSSValue* ConsumePrefixedBackgroundBox(CSSParserTokenRange& range,
                                              const CSSParserContext* context,
                                              bool allow_text_value) {
  // The values 'border', 'padding' and 'content' are deprecated and do not
  // apply to the version of the property that has the -webkit- prefix removed.
  if (CSSValue* value =
          ConsumeIdentRange(range, CSSValueBorder, CSSValuePaddingBox))
    return value;
  if (allow_text_value && range.Peek().Id() == CSSValueText)
    return ConsumeIdent(range);
  return nullptr;
}

static CSSValue* ConsumeBackgroundSize(CSSParserTokenRange& range,
                                       CSSParserMode css_parser_mode,
                                       bool use_legacy_parsing) {
  if (IdentMatches<CSSValueContain, CSSValueCover>(range.Peek().Id()))
    return ConsumeIdent(range);

  CSSValue* horizontal = ConsumeIdent<CSSValueAuto>(range);
  if (!horizontal)
    horizontal = ConsumeLengthOrPercent(range, css_parser_mode, kValueRangeAll,
                                        UnitlessQuirk::kForbid);

  CSSValue* vertical = nullptr;
  if (!range.AtEnd()) {
    if (range.Peek().Id() == CSSValueAuto)  // `auto' is the default
      range.ConsumeIncludingWhitespace();
    else
      vertical = ConsumeLengthOrPercent(range, css_parser_mode, kValueRangeAll,
                                        UnitlessQuirk::kForbid);
  } else if (use_legacy_parsing) {
    // Legacy syntax: "-webkit-background-size: 10px" is equivalent to
    // "background-size: 10px 10px".
    vertical = horizontal;
  }
  if (!vertical)
    return horizontal;
  return CSSValuePair::Create(horizontal, vertical,
                              CSSValuePair::kKeepIdenticalValues);
}

static CSSValue* ConsumeBackgroundComponent(CSSPropertyID unresolved_property,
                                            CSSParserTokenRange& range,
                                            const CSSParserContext* context) {
  switch (unresolved_property) {
    case CSSPropertyBackgroundClip:
      return CSSPropertyBackgroundUtils::ConsumeBackgroundBox(range);
    case CSSPropertyBackgroundBlendMode:
      return CSSPropertyBackgroundUtils::ConsumeBackgroundBlendMode(range);
    case CSSPropertyBackgroundAttachment:
      return CSSPropertyBackgroundUtils::ConsumeBackgroundAttachment(range);
    case CSSPropertyBackgroundOrigin:
      return CSSPropertyBackgroundUtils::ConsumeBackgroundBox(range);
    case CSSPropertyWebkitMaskComposite:
      return CSSPropertyBackgroundUtils::ConsumeBackgroundComposite(range);
    case CSSPropertyMaskSourceType:
      return CSSPropertyBackgroundUtils::ConsumeMaskSourceType(range);
    case CSSPropertyWebkitBackgroundClip:
    case CSSPropertyWebkitMaskClip:
      return ConsumePrefixedBackgroundBox(range, context,
                                          true /* allow_text_value */);
    case CSSPropertyWebkitBackgroundOrigin:
    case CSSPropertyWebkitMaskOrigin:
      return ConsumePrefixedBackgroundBox(range, context,
                                          false /* allow_text_value */);
    case CSSPropertyBackgroundImage:
    case CSSPropertyWebkitMaskImage:
      return ConsumeImageOrNone(range, context);
    case CSSPropertyBackgroundPositionX:
    case CSSPropertyWebkitMaskPositionX:
      return CSSPropertyPositionUtils::ConsumePositionLonghand<CSSValueLeft,
                                                               CSSValueRight>(
          range, context->Mode());
    case CSSPropertyBackgroundPositionY:
    case CSSPropertyWebkitMaskPositionY:
      return CSSPropertyPositionUtils::ConsumePositionLonghand<CSSValueTop,
                                                               CSSValueBottom>(
          range, context->Mode());
    case CSSPropertyBackgroundSize:
    case CSSPropertyWebkitMaskSize:
      return ConsumeBackgroundSize(range, context->Mode(),
                                   false /* use_legacy_parsing */);
    case CSSPropertyAliasWebkitBackgroundSize:
      return ConsumeBackgroundSize(range, context->Mode(),
                                   true /* use_legacy_parsing */);
    case CSSPropertyBackgroundColor:
      return ConsumeColor(range, context->Mode());
    default:
      break;
  };
  return nullptr;
}

static CSSValue* ConsumeFitContent(CSSParserTokenRange& range,
                                   CSSParserMode css_parser_mode) {
  CSSParserTokenRange range_copy = range;
  CSSParserTokenRange args = ConsumeFunction(range_copy);
  CSSPrimitiveValue* length = ConsumeLengthOrPercent(
      args, css_parser_mode, kValueRangeNonNegative, UnitlessQuirk::kAllow);
  if (!length || !args.AtEnd())
    return nullptr;
  range = range_copy;
  CSSFunctionValue* result = CSSFunctionValue::Create(CSSValueFitContent);
  result->Append(*length);
  return result;
}

static bool IsGridBreadthFixedSized(const CSSValue& value) {
  if (value.IsIdentifierValue()) {
    CSSValueID value_id = ToCSSIdentifierValue(value).GetValueID();
    return !(value_id == CSSValueMinContent || value_id == CSSValueMaxContent ||
             value_id == CSSValueAuto);
  }

  if (value.IsPrimitiveValue()) {
    return !ToCSSPrimitiveValue(value).IsFlex();
  }

  NOTREACHED();
  return true;
}

static bool IsGridTrackFixedSized(const CSSValue& value) {
  if (value.IsPrimitiveValue() || value.IsIdentifierValue())
    return IsGridBreadthFixedSized(value);

  DCHECK(value.IsFunctionValue());
  auto& function = ToCSSFunctionValue(value);
  if (function.FunctionType() == CSSValueFitContent)
    return false;

  const CSSValue& min_value = function.Item(0);
  const CSSValue& max_value = function.Item(1);
  return IsGridBreadthFixedSized(min_value) ||
         IsGridBreadthFixedSized(max_value);
}

static CSSValue* ConsumeGridBreadth(CSSParserTokenRange& range,
                                    CSSParserMode css_parser_mode) {
  const CSSParserToken& token = range.Peek();
  if (IdentMatches<CSSValueMinContent, CSSValueMaxContent, CSSValueAuto>(
          token.Id()))
    return ConsumeIdent(range);
  if (token.GetType() == kDimensionToken &&
      token.GetUnitType() == CSSPrimitiveValue::UnitType::kFraction) {
    if (range.Peek().NumericValue() < 0)
      return nullptr;
    return CSSPrimitiveValue::Create(
        range.ConsumeIncludingWhitespace().NumericValue(),
        CSSPrimitiveValue::UnitType::kFraction);
  }
  return ConsumeLengthOrPercent(range, css_parser_mode, kValueRangeNonNegative,
                                UnitlessQuirk::kAllow);
}

static CSSValue* ConsumeGridTrackSize(CSSParserTokenRange& range,
                                      CSSParserMode css_parser_mode) {
  const CSSParserToken& token = range.Peek();
  if (IdentMatches<CSSValueAuto>(token.Id()))
    return ConsumeIdent(range);

  if (token.FunctionId() == CSSValueMinmax) {
    CSSParserTokenRange range_copy = range;
    CSSParserTokenRange args = ConsumeFunction(range_copy);
    CSSValue* min_track_breadth = ConsumeGridBreadth(args, css_parser_mode);
    if (!min_track_breadth ||
        (min_track_breadth->IsPrimitiveValue() &&
         ToCSSPrimitiveValue(min_track_breadth)->IsFlex()) ||
        !ConsumeCommaIncludingWhitespace(args))
      return nullptr;
    CSSValue* max_track_breadth = ConsumeGridBreadth(args, css_parser_mode);
    if (!max_track_breadth || !args.AtEnd())
      return nullptr;
    range = range_copy;
    CSSFunctionValue* result = CSSFunctionValue::Create(CSSValueMinmax);
    result->Append(*min_track_breadth);
    result->Append(*max_track_breadth);
    return result;
  }

  if (token.FunctionId() == CSSValueFitContent)
    return ConsumeFitContent(range, css_parser_mode);

  return ConsumeGridBreadth(range, css_parser_mode);
}

// Appends to the passed in CSSGridLineNamesValue if any, otherwise creates a
// new one.
static CSSGridLineNamesValue* ConsumeGridLineNames(
    CSSParserTokenRange& range,
    CSSGridLineNamesValue* line_names = nullptr) {
  CSSParserTokenRange range_copy = range;
  if (range_copy.ConsumeIncludingWhitespace().GetType() != kLeftBracketToken)
    return nullptr;
  if (!line_names)
    line_names = CSSGridLineNamesValue::Create();
  while (CSSCustomIdentValue* line_name =
             CSSPropertyGridUtils::ConsumeCustomIdentForGridLine(range_copy))
    line_names->Append(*line_name);
  if (range_copy.ConsumeIncludingWhitespace().GetType() != kRightBracketToken)
    return nullptr;
  range = range_copy;
  return line_names;
}

static bool ConsumeGridTrackRepeatFunction(CSSParserTokenRange& range,
                                           CSSParserMode css_parser_mode,
                                           CSSValueList& list,
                                           bool& is_auto_repeat,
                                           bool& all_tracks_are_fixed_sized) {
  CSSParserTokenRange args = ConsumeFunction(range);
  // The number of repetitions for <auto-repeat> is not important at parsing
  // level because it will be computed later, let's set it to 1.
  size_t repetitions = 1;
  is_auto_repeat =
      IdentMatches<CSSValueAutoFill, CSSValueAutoFit>(args.Peek().Id());
  CSSValueList* repeated_values;
  if (is_auto_repeat) {
    repeated_values =
        CSSGridAutoRepeatValue::Create(args.ConsumeIncludingWhitespace().Id());
  } else {
    // TODO(rob.buis): a consumeIntegerRaw would be more efficient here.
    CSSPrimitiveValue* repetition = ConsumePositiveInteger(args);
    if (!repetition)
      return false;
    repetitions =
        clampTo<size_t>(repetition->GetDoubleValue(), 0, kGridMaxTracks);
    repeated_values = CSSValueList::CreateSpaceSeparated();
  }
  if (!ConsumeCommaIncludingWhitespace(args))
    return false;
  CSSGridLineNamesValue* line_names = ConsumeGridLineNames(args);
  if (line_names)
    repeated_values->Append(*line_names);

  size_t number_of_tracks = 0;
  while (!args.AtEnd()) {
    CSSValue* track_size = ConsumeGridTrackSize(args, css_parser_mode);
    if (!track_size)
      return false;
    if (all_tracks_are_fixed_sized)
      all_tracks_are_fixed_sized = IsGridTrackFixedSized(*track_size);
    repeated_values->Append(*track_size);
    ++number_of_tracks;
    line_names = ConsumeGridLineNames(args);
    if (line_names)
      repeated_values->Append(*line_names);
  }
  // We should have found at least one <track-size> or else it is not a valid
  // <track-list>.
  if (!number_of_tracks)
    return false;

  if (is_auto_repeat) {
    list.Append(*repeated_values);
  } else {
    // We clamp the repetitions to a multiple of the repeat() track list's size,
    // while staying below the max grid size.
    repetitions = std::min(repetitions, kGridMaxTracks / number_of_tracks);
    for (size_t i = 0; i < repetitions; ++i) {
      for (size_t j = 0; j < repeated_values->length(); ++j)
        list.Append(repeated_values->Item(j));
    }
  }
  return true;
}

enum TrackListType { kGridTemplate, kGridTemplateNoRepeat, kGridAuto };

static CSSValue* ConsumeGridTrackList(CSSParserTokenRange& range,
                                      CSSParserMode css_parser_mode,
                                      TrackListType track_list_type) {
  bool allow_grid_line_names = track_list_type != kGridAuto;
  CSSValueList* values = CSSValueList::CreateSpaceSeparated();
  CSSGridLineNamesValue* line_names = ConsumeGridLineNames(range);
  if (line_names) {
    if (!allow_grid_line_names)
      return nullptr;
    values->Append(*line_names);
  }

  bool allow_repeat = track_list_type == kGridTemplate;
  bool seen_auto_repeat = false;
  bool all_tracks_are_fixed_sized = true;
  do {
    bool is_auto_repeat;
    if (range.Peek().FunctionId() == CSSValueRepeat) {
      if (!allow_repeat)
        return nullptr;
      if (!ConsumeGridTrackRepeatFunction(range, css_parser_mode, *values,
                                          is_auto_repeat,
                                          all_tracks_are_fixed_sized))
        return nullptr;
      if (is_auto_repeat && seen_auto_repeat)
        return nullptr;
      seen_auto_repeat = seen_auto_repeat || is_auto_repeat;
    } else if (CSSValue* value = ConsumeGridTrackSize(range, css_parser_mode)) {
      if (all_tracks_are_fixed_sized)
        all_tracks_are_fixed_sized = IsGridTrackFixedSized(*value);
      values->Append(*value);
    } else {
      return nullptr;
    }
    if (seen_auto_repeat && !all_tracks_are_fixed_sized)
      return nullptr;
    line_names = ConsumeGridLineNames(range);
    if (line_names) {
      if (!allow_grid_line_names)
        return nullptr;
      values->Append(*line_names);
    }
  } while (!range.AtEnd() && range.Peek().GetType() != kDelimiterToken);
  return values;
}

static CSSValue* ConsumeGridTemplatesRowsOrColumns(
    CSSParserTokenRange& range,
    CSSParserMode css_parser_mode) {
  if (range.Peek().Id() == CSSValueNone)
    return ConsumeIdent(range);
  return ConsumeGridTrackList(range, css_parser_mode, kGridTemplate);
}

const CSSValue* CSSPropertyParser::ParseSingleValue(
    CSSPropertyID unresolved_property,
    CSSPropertyID current_shorthand) {
  DCHECK(context_);

  // Gets the parsing method for our current property from the property API.
  // If it has been implemented, we call this method, otherwise we manually
  // parse this value in the switch statement below. As we implement APIs for
  // other properties, those properties will be taken out of the switch
  // statement.
  bool needs_legacy_parsing = false;
  const CSSValue* const parsed_value =
      CSSPropertyParserHelpers::ParseLonghandViaAPI(
          unresolved_property, current_shorthand, *context_, range_,
          needs_legacy_parsing);
  if (!needs_legacy_parsing)
    return parsed_value;

  CSSPropertyID property = resolveCSSPropertyID(unresolved_property);
  switch (property) {
    case CSSPropertyMaxWidth:
    case CSSPropertyMaxHeight:
      return CSSPropertyLengthUtils::ConsumeMaxWidthOrHeight(
          range_, *context_, UnitlessQuirk::kAllow);
    case CSSPropertyMinWidth:
    case CSSPropertyMinHeight:
    case CSSPropertyWidth:
    case CSSPropertyHeight:
      return CSSPropertyLengthUtils::ConsumeWidthOrHeight(
          range_, *context_, UnitlessQuirk::kAllow);
    case CSSPropertyInlineSize:
    case CSSPropertyBlockSize:
    case CSSPropertyMinInlineSize:
    case CSSPropertyMinBlockSize:
    case CSSPropertyWebkitMinLogicalWidth:
    case CSSPropertyWebkitMinLogicalHeight:
    case CSSPropertyWebkitLogicalWidth:
    case CSSPropertyWebkitLogicalHeight:
      return CSSPropertyLengthUtils::ConsumeWidthOrHeight(range_, *context_);
    case CSSPropertyTextDecoration:
      DCHECK(!RuntimeEnabledFeatures::CSS3TextDecorationsEnabled());
      return CSSPropertyTextDecorationLineUtils::ConsumeTextDecorationLine(
          range_);
    case CSSPropertyBackgroundPositionX:
    case CSSPropertyWebkitMaskPositionX:
      return ConsumeCommaSeparatedList(
          CSSPropertyPositionUtils::ConsumePositionLonghand<CSSValueLeft,
                                                            CSSValueRight>,
          range_, context_->Mode());
    case CSSPropertyBackgroundPositionY:
    case CSSPropertyWebkitMaskPositionY:
      return ConsumeCommaSeparatedList(
          CSSPropertyPositionUtils::ConsumePositionLonghand<CSSValueTop,
                                                            CSSValueBottom>,
          range_, context_->Mode());
    case CSSPropertyBackgroundSize:
    case CSSPropertyWebkitMaskSize:
      return ConsumeCommaSeparatedList(ConsumeBackgroundSize, range_,
                                       context_->Mode(),
                                       isPropertyAlias(unresolved_property));
    case CSSPropertyWebkitBackgroundClip:
    case CSSPropertyWebkitMaskClip:
      return ConsumeCommaSeparatedList(ConsumePrefixedBackgroundBox, range_,
                                       context_, true /* allow_text_value */);
    case CSSPropertyWebkitBackgroundOrigin:
    case CSSPropertyWebkitMaskOrigin:
      return ConsumeCommaSeparatedList(ConsumePrefixedBackgroundBox, range_,
                                       context_, false /* allow_text_value */);
    case CSSPropertyWebkitMaskRepeatX:
    case CSSPropertyWebkitMaskRepeatY:
      return nullptr;
    case CSSPropertyGridColumnEnd:
    case CSSPropertyGridColumnStart:
    case CSSPropertyGridRowEnd:
    case CSSPropertyGridRowStart:
      DCHECK(RuntimeEnabledFeatures::CSSGridLayoutEnabled());
      return CSSPropertyGridUtils::ConsumeGridLine(range_);
    case CSSPropertyGridAutoColumns:
    case CSSPropertyGridAutoRows:
      DCHECK(RuntimeEnabledFeatures::CSSGridLayoutEnabled());
      return ConsumeGridTrackList(range_, context_->Mode(), kGridAuto);
    case CSSPropertyGridTemplateColumns:
    case CSSPropertyGridTemplateRows:
      DCHECK(RuntimeEnabledFeatures::CSSGridLayoutEnabled());
      return ConsumeGridTemplatesRowsOrColumns(range_, context_->Mode());
    default:
      return nullptr;
  }
}

static CSSIdentifierValue* ConsumeFontDisplay(CSSParserTokenRange& range) {
  return ConsumeIdent<CSSValueAuto, CSSValueBlock, CSSValueSwap,
                      CSSValueFallback, CSSValueOptional>(range);
}

static CSSValueList* ConsumeFontFaceUnicodeRange(CSSParserTokenRange& range) {
  CSSValueList* values = CSSValueList::CreateCommaSeparated();

  do {
    const CSSParserToken& token = range.ConsumeIncludingWhitespace();
    if (token.GetType() != kUnicodeRangeToken)
      return nullptr;

    UChar32 start = token.UnicodeRangeStart();
    UChar32 end = token.UnicodeRangeEnd();
    if (start > end)
      return nullptr;
    values->Append(*CSSUnicodeRangeValue::Create(start, end));
  } while (ConsumeCommaIncludingWhitespace(range));

  return values;
}

static CSSValue* ConsumeFontFaceSrcURI(CSSParserTokenRange& range,
                                       const CSSParserContext* context) {
  String url = ConsumeUrlAsStringView(range).ToString();
  if (url.IsNull())
    return nullptr;
  CSSFontFaceSrcValue* uri_value(CSSFontFaceSrcValue::Create(
      url, context->CompleteURL(url), context->GetReferrer(),
      context->ShouldCheckContentSecurityPolicy()));

  if (range.Peek().FunctionId() != CSSValueFormat)
    return uri_value;

  // FIXME: https://drafts.csswg.org/css-fonts says that format() contains a
  // comma-separated list of strings, but CSSFontFaceSrcValue stores only one
  // format. Allowing one format for now.
  CSSParserTokenRange args = ConsumeFunction(range);
  const CSSParserToken& arg = args.ConsumeIncludingWhitespace();
  if ((arg.GetType() != kStringToken) || !args.AtEnd())
    return nullptr;
  uri_value->SetFormat(arg.Value().ToString());
  return uri_value;
}

static CSSValue* ConsumeFontFaceSrcLocal(CSSParserTokenRange& range,
                                         const CSSParserContext* context) {
  CSSParserTokenRange args = ConsumeFunction(range);
  ContentSecurityPolicyDisposition should_check_content_security_policy =
      context->ShouldCheckContentSecurityPolicy();
  if (args.Peek().GetType() == kStringToken) {
    const CSSParserToken& arg = args.ConsumeIncludingWhitespace();
    if (!args.AtEnd())
      return nullptr;
    return CSSFontFaceSrcValue::CreateLocal(
        arg.Value().ToString(), should_check_content_security_policy);
  }
  if (args.Peek().GetType() == kIdentToken) {
    String family_name = CSSPropertyFontUtils::ConcatenateFamilyName(args);
    if (!args.AtEnd())
      return nullptr;
    return CSSFontFaceSrcValue::CreateLocal(
        family_name, should_check_content_security_policy);
  }
  return nullptr;
}

static CSSValueList* ConsumeFontFaceSrc(CSSParserTokenRange& range,
                                        const CSSParserContext* context) {
  CSSValueList* values = CSSValueList::CreateCommaSeparated();

  do {
    const CSSParserToken& token = range.Peek();
    CSSValue* parsed_value = nullptr;
    if (token.FunctionId() == CSSValueLocal)
      parsed_value = ConsumeFontFaceSrcLocal(range, context);
    else
      parsed_value = ConsumeFontFaceSrcURI(range, context);
    if (!parsed_value)
      return nullptr;
    values->Append(*parsed_value);
  } while (ConsumeCommaIncludingWhitespace(range));
  return values;
}

bool CSSPropertyParser::ParseFontFaceDescriptor(CSSPropertyID prop_id) {
  CSSValue* parsed_value = nullptr;
  switch (prop_id) {
    case CSSPropertyFontFamily:
      if (CSSPropertyFontUtils::ConsumeGenericFamily(range_))
        return false;
      parsed_value = CSSPropertyFontUtils::ConsumeFamilyName(range_);
      break;
    case CSSPropertySrc:  // This is a list of urls or local references.
      parsed_value = ConsumeFontFaceSrc(range_, context_);
      break;
    case CSSPropertyUnicodeRange:
      parsed_value = ConsumeFontFaceUnicodeRange(range_);
      break;
    case CSSPropertyFontDisplay:
      parsed_value = ConsumeFontDisplay(range_);
      break;
    case CSSPropertyFontStretch:
      parsed_value = CSSPropertyFontUtils::ConsumeFontStretch(range_);
      break;
    case CSSPropertyFontStyle:
      parsed_value = CSSPropertyFontUtils::ConsumeFontStyle(range_);
      break;
    case CSSPropertyFontVariant:
      parsed_value = ConsumeFontVariantList(range_);
      break;
    case CSSPropertyFontWeight:
      parsed_value = CSSPropertyFontUtils::ConsumeFontWeight(range_);
      break;
    case CSSPropertyFontFeatureSettings:
      parsed_value = CSSPropertyFontUtils::ConsumeFontFeatureSettings(range_);
      break;
    default:
      break;
  }

  if (!parsed_value || !range_.AtEnd())
    return false;

  AddParsedProperty(prop_id, CSSPropertyInvalid, *parsed_value, false);
  return true;
}

static CSSValue* ConsumeSingleViewportDescriptor(
    CSSParserTokenRange& range,
    CSSPropertyID prop_id,
    CSSParserMode css_parser_mode) {
  CSSValueID id = range.Peek().Id();
  switch (prop_id) {
    case CSSPropertyMinWidth:
    case CSSPropertyMaxWidth:
    case CSSPropertyMinHeight:
    case CSSPropertyMaxHeight:
      if (id == CSSValueAuto || id == CSSValueInternalExtendToZoom)
        return ConsumeIdent(range);
      return ConsumeLengthOrPercent(range, css_parser_mode,
                                    kValueRangeNonNegative);
    case CSSPropertyMinZoom:
    case CSSPropertyMaxZoom:
    case CSSPropertyZoom: {
      if (id == CSSValueAuto)
        return ConsumeIdent(range);
      CSSValue* parsed_value = ConsumeNumber(range, kValueRangeNonNegative);
      if (parsed_value)
        return parsed_value;
      return ConsumePercent(range, kValueRangeNonNegative);
    }
    case CSSPropertyUserZoom:
      return ConsumeIdent<CSSValueZoom, CSSValueFixed>(range);
    case CSSPropertyOrientation:
      return ConsumeIdent<CSSValueAuto, CSSValuePortrait, CSSValueLandscape>(
          range);
    default:
      NOTREACHED();
      break;
  }

  NOTREACHED();
  return nullptr;
}

bool CSSPropertyParser::ParseViewportDescriptor(CSSPropertyID prop_id,
                                                bool important) {
  DCHECK(RuntimeEnabledFeatures::CSSViewportEnabled() ||
         IsUASheetBehavior(context_->Mode()));

  switch (prop_id) {
    case CSSPropertyWidth: {
      CSSValue* min_width = ConsumeSingleViewportDescriptor(
          range_, CSSPropertyMinWidth, context_->Mode());
      if (!min_width)
        return false;
      CSSValue* max_width = min_width;
      if (!range_.AtEnd()) {
        max_width = ConsumeSingleViewportDescriptor(range_, CSSPropertyMaxWidth,
                                                    context_->Mode());
      }
      if (!max_width || !range_.AtEnd())
        return false;
      AddParsedProperty(CSSPropertyMinWidth, CSSPropertyInvalid, *min_width,
                        important);
      AddParsedProperty(CSSPropertyMaxWidth, CSSPropertyInvalid, *max_width,
                        important);
      return true;
    }
    case CSSPropertyHeight: {
      CSSValue* min_height = ConsumeSingleViewportDescriptor(
          range_, CSSPropertyMinHeight, context_->Mode());
      if (!min_height)
        return false;
      CSSValue* max_height = min_height;
      if (!range_.AtEnd()) {
        max_height = ConsumeSingleViewportDescriptor(
            range_, CSSPropertyMaxHeight, context_->Mode());
      }
      if (!max_height || !range_.AtEnd())
        return false;
      AddParsedProperty(CSSPropertyMinHeight, CSSPropertyInvalid, *min_height,
                        important);
      AddParsedProperty(CSSPropertyMaxHeight, CSSPropertyInvalid, *max_height,
                        important);
      return true;
    }
    case CSSPropertyMinWidth:
    case CSSPropertyMaxWidth:
    case CSSPropertyMinHeight:
    case CSSPropertyMaxHeight:
    case CSSPropertyMinZoom:
    case CSSPropertyMaxZoom:
    case CSSPropertyZoom:
    case CSSPropertyUserZoom:
    case CSSPropertyOrientation: {
      CSSValue* parsed_value =
          ConsumeSingleViewportDescriptor(range_, prop_id, context_->Mode());
      if (!parsed_value || !range_.AtEnd())
        return false;
      AddParsedProperty(prop_id, CSSPropertyInvalid, *parsed_value, important);
      return true;
    }
    default:
      return false;
  }
}

bool CSSPropertyParser::ConsumeBorder(bool important) {
  CSSValue* width = nullptr;
  const CSSValue* style = nullptr;
  CSSValue* color = nullptr;

  while (!width || !style || !color) {
    if (!width) {
      width =
          ConsumeLineWidth(range_, context_->Mode(), UnitlessQuirk::kForbid);
      if (width)
        continue;
    }
    if (!style) {
      style = ParseSingleValue(CSSPropertyBorderLeftStyle, CSSPropertyBorder);
      if (style)
        continue;
    }
    if (!color) {
      color = ConsumeColor(range_, context_->Mode());
      if (color)
        continue;
    }
    break;
  }

  if (!width && !style && !color)
    return false;

  if (!width)
    width = CSSInitialValue::Create();
  if (!style)
    style = CSSInitialValue::Create();
  if (!color)
    color = CSSInitialValue::Create();

  AddExpandedPropertyForValue(CSSPropertyBorderWidth, *width, important);
  AddExpandedPropertyForValue(CSSPropertyBorderStyle, *style, important);
  AddExpandedPropertyForValue(CSSPropertyBorderColor, *color, important);
  AddExpandedPropertyForValue(CSSPropertyBorderImage,
                              *CSSInitialValue::Create(), important);

  return range_.AtEnd();
}

static bool ConsumeRepeatStyleComponent(CSSParserTokenRange& range,
                                        CSSValue*& value1,
                                        CSSValue*& value2,
                                        bool& implicit) {
  if (ConsumeIdent<CSSValueRepeatX>(range)) {
    value1 = CSSIdentifierValue::Create(CSSValueRepeat);
    value2 = CSSIdentifierValue::Create(CSSValueNoRepeat);
    implicit = true;
    return true;
  }
  if (ConsumeIdent<CSSValueRepeatY>(range)) {
    value1 = CSSIdentifierValue::Create(CSSValueNoRepeat);
    value2 = CSSIdentifierValue::Create(CSSValueRepeat);
    implicit = true;
    return true;
  }
  value1 = ConsumeIdent<CSSValueRepeat, CSSValueNoRepeat, CSSValueRound,
                        CSSValueSpace>(range);
  if (!value1)
    return false;

  value2 = ConsumeIdent<CSSValueRepeat, CSSValueNoRepeat, CSSValueRound,
                        CSSValueSpace>(range);
  if (!value2) {
    value2 = value1;
    implicit = true;
  }
  return true;
}

static bool ConsumeRepeatStyle(CSSParserTokenRange& range,
                               CSSValue*& result_x,
                               CSSValue*& result_y,
                               bool& implicit) {
  do {
    CSSValue* repeat_x = nullptr;
    CSSValue* repeat_y = nullptr;
    if (!ConsumeRepeatStyleComponent(range, repeat_x, repeat_y, implicit))
      return false;
    CSSPropertyBackgroundUtils::AddBackgroundValue(result_x, repeat_x);
    CSSPropertyBackgroundUtils::AddBackgroundValue(result_y, repeat_y);
  } while (ConsumeCommaIncludingWhitespace(range));
  return true;
}

// Note: consumeBackgroundShorthand assumes y properties (for example
// background-position-y) follow the x properties in the shorthand array.
bool CSSPropertyParser::ConsumeBackgroundShorthand(
    const StylePropertyShorthand& shorthand,
    bool important) {
  const unsigned longhand_count = shorthand.length();
  CSSValue* longhands[10] = {0};
  DCHECK_LE(longhand_count, 10u);

  bool implicit = false;
  do {
    bool parsed_longhand[10] = {false};
    CSSValue* origin_value = nullptr;
    do {
      bool found_property = false;
      for (size_t i = 0; i < longhand_count; ++i) {
        if (parsed_longhand[i])
          continue;

        CSSValue* value = nullptr;
        CSSValue* value_y = nullptr;
        CSSPropertyID property = shorthand.properties()[i];
        if (property == CSSPropertyBackgroundRepeatX ||
            property == CSSPropertyWebkitMaskRepeatX) {
          ConsumeRepeatStyleComponent(range_, value, value_y, implicit);
        } else if (property == CSSPropertyBackgroundPositionX ||
                   property == CSSPropertyWebkitMaskPositionX) {
          if (!ConsumePosition(range_, *context_, UnitlessQuirk::kForbid,
                               WebFeature::kThreeValuedPositionBackground,
                               value, value_y))
            continue;
        } else if (property == CSSPropertyBackgroundSize ||
                   property == CSSPropertyWebkitMaskSize) {
          if (!ConsumeSlashIncludingWhitespace(range_))
            continue;
          value = ConsumeBackgroundSize(range_, context_->Mode(),
                                        false /* use_legacy_parsing */);
          if (!value ||
              !parsed_longhand[i - 1])  // Position must have been
                                        // parsed in the current layer.
            return false;
        } else if (property == CSSPropertyBackgroundPositionY ||
                   property == CSSPropertyBackgroundRepeatY ||
                   property == CSSPropertyWebkitMaskPositionY ||
                   property == CSSPropertyWebkitMaskRepeatY) {
          continue;
        } else {
          value = ConsumeBackgroundComponent(property, range_, context_);
        }
        if (value) {
          if (property == CSSPropertyBackgroundOrigin ||
              property == CSSPropertyWebkitMaskOrigin)
            origin_value = value;
          parsed_longhand[i] = true;
          found_property = true;
          CSSPropertyBackgroundUtils::AddBackgroundValue(longhands[i], value);
          if (value_y) {
            parsed_longhand[i + 1] = true;
            CSSPropertyBackgroundUtils::AddBackgroundValue(longhands[i + 1],
                                                           value_y);
          }
        }
      }
      if (!found_property)
        return false;
    } while (!range_.AtEnd() && range_.Peek().GetType() != kCommaToken);

    // TODO(timloh): This will make invalid longhands, see crbug.com/386459
    for (size_t i = 0; i < longhand_count; ++i) {
      CSSPropertyID property = shorthand.properties()[i];
      if (property == CSSPropertyBackgroundColor && !range_.AtEnd()) {
        if (parsed_longhand[i])
          return false;  // Colors are only allowed in the last layer.
        continue;
      }
      if ((property == CSSPropertyBackgroundClip ||
           property == CSSPropertyWebkitMaskClip) &&
          !parsed_longhand[i] && origin_value) {
        CSSPropertyBackgroundUtils::AddBackgroundValue(longhands[i],
                                                       origin_value);
        continue;
      }
      if (!parsed_longhand[i]) {
        CSSPropertyBackgroundUtils::AddBackgroundValue(
            longhands[i], CSSInitialValue::Create());
      }
    }
  } while (ConsumeCommaIncludingWhitespace(range_));
  if (!range_.AtEnd())
    return false;

  for (size_t i = 0; i < longhand_count; ++i) {
    CSSPropertyID property = shorthand.properties()[i];
    if (property == CSSPropertyBackgroundSize && longhands[i] &&
        context_->UseLegacyBackgroundSizeShorthandBehavior())
      continue;
    AddParsedProperty(property, shorthand.id(), *longhands[i], important,
                      implicit);
  }
  return true;
}

bool CSSPropertyParser::ConsumeGridTemplateRowsAndAreasAndColumns(
    CSSPropertyID shorthand_id,
    bool important) {
  NamedGridAreaMap grid_area_map;
  size_t row_count = 0;
  size_t column_count = 0;
  CSSValueList* template_rows = CSSValueList::CreateSpaceSeparated();

  // Persists between loop iterations so we can use the same value for
  // consecutive <line-names> values
  CSSGridLineNamesValue* line_names = nullptr;

  do {
    // Handle leading <custom-ident>*.
    bool has_previous_line_names = line_names;
    line_names = ConsumeGridLineNames(range_, line_names);
    if (line_names && !has_previous_line_names)
      template_rows->Append(*line_names);

    // Handle a template-area's row.
    if (range_.Peek().GetType() != kStringToken ||
        !CSSPropertyGridTemplateAreasUtils::ParseGridTemplateAreasRow(
            range_.ConsumeIncludingWhitespace().Value().ToString(),
            grid_area_map, row_count, column_count))
      return false;
    ++row_count;

    // Handle template-rows's track-size.
    CSSValue* value = ConsumeGridTrackSize(range_, context_->Mode());
    if (!value)
      value = CSSIdentifierValue::Create(CSSValueAuto);
    template_rows->Append(*value);

    // This will handle the trailing/leading <custom-ident>* in the grammar.
    line_names = ConsumeGridLineNames(range_);
    if (line_names)
      template_rows->Append(*line_names);
  } while (!range_.AtEnd() && !(range_.Peek().GetType() == kDelimiterToken &&
                                range_.Peek().Delimiter() == '/'));

  CSSValue* columns_value = nullptr;
  if (!range_.AtEnd()) {
    if (!ConsumeSlashIncludingWhitespace(range_))
      return false;
    columns_value =
        ConsumeGridTrackList(range_, context_->Mode(), kGridTemplateNoRepeat);
    if (!columns_value || !range_.AtEnd())
      return false;
  } else {
    columns_value = CSSIdentifierValue::Create(CSSValueNone);
  }
  AddParsedProperty(CSSPropertyGridTemplateRows, shorthand_id, *template_rows,
                    important);
  AddParsedProperty(CSSPropertyGridTemplateColumns, shorthand_id,
                    *columns_value, important);
  AddParsedProperty(CSSPropertyGridTemplateAreas, shorthand_id,
                    *CSSGridTemplateAreasValue::Create(grid_area_map, row_count,
                                                       column_count),
                    important);
  return true;
}

bool CSSPropertyParser::ConsumeGridTemplateShorthand(CSSPropertyID shorthand_id,
                                                     bool important) {
  DCHECK(RuntimeEnabledFeatures::CSSGridLayoutEnabled());
  DCHECK_EQ(gridTemplateShorthand().length(), 3u);

  CSSParserTokenRange range_copy = range_;
  CSSValue* rows_value = ConsumeIdent<CSSValueNone>(range_);

  // 1- 'none' case.
  if (rows_value && range_.AtEnd()) {
    AddParsedProperty(CSSPropertyGridTemplateRows, shorthand_id,
                      *CSSIdentifierValue::Create(CSSValueNone), important);
    AddParsedProperty(CSSPropertyGridTemplateColumns, shorthand_id,
                      *CSSIdentifierValue::Create(CSSValueNone), important);
    AddParsedProperty(CSSPropertyGridTemplateAreas, shorthand_id,
                      *CSSIdentifierValue::Create(CSSValueNone), important);
    return true;
  }

  // 2- <grid-template-rows> / <grid-template-columns>
  if (!rows_value)
    rows_value = ConsumeGridTrackList(range_, context_->Mode(), kGridTemplate);

  if (rows_value) {
    if (!ConsumeSlashIncludingWhitespace(range_))
      return false;
    CSSValue* columns_value =
        ConsumeGridTemplatesRowsOrColumns(range_, context_->Mode());
    if (!columns_value || !range_.AtEnd())
      return false;

    AddParsedProperty(CSSPropertyGridTemplateRows, shorthand_id, *rows_value,
                      important);
    AddParsedProperty(CSSPropertyGridTemplateColumns, shorthand_id,
                      *columns_value, important);
    AddParsedProperty(CSSPropertyGridTemplateAreas, shorthand_id,
                      *CSSIdentifierValue::Create(CSSValueNone), important);
    return true;
  }

  // 3- [ <line-names>? <string> <track-size>? <line-names>? ]+
  // [ / <track-list> ]?
  range_ = range_copy;
  return ConsumeGridTemplateRowsAndAreasAndColumns(shorthand_id, important);
}

static CSSValueList* ConsumeImplicitAutoFlow(CSSParserTokenRange& range,
                                             const CSSValue& flow_direction) {
  // [ auto-flow && dense? ]
  CSSValue* dense_algorithm = nullptr;
  if ((ConsumeIdent<CSSValueAutoFlow>(range))) {
    dense_algorithm = ConsumeIdent<CSSValueDense>(range);
  } else {
    dense_algorithm = ConsumeIdent<CSSValueDense>(range);
    if (!dense_algorithm)
      return nullptr;
    if (!ConsumeIdent<CSSValueAutoFlow>(range))
      return nullptr;
  }
  CSSValueList* list = CSSValueList::CreateSpaceSeparated();
  list->Append(flow_direction);
  if (dense_algorithm)
    list->Append(*dense_algorithm);
  return list;
}

bool CSSPropertyParser::ConsumeGridShorthand(bool important) {
  DCHECK(RuntimeEnabledFeatures::CSSGridLayoutEnabled());
  DCHECK_EQ(shorthandForProperty(CSSPropertyGrid).length(), 8u);

  CSSParserTokenRange range_copy = range_;

  // 1- <grid-template>
  if (ConsumeGridTemplateShorthand(CSSPropertyGrid, important)) {
    // It can only be specified the explicit or the implicit grid properties in
    // a single grid declaration. The sub-properties not specified are set to
    // their initial value, as normal for shorthands.
    AddParsedProperty(CSSPropertyGridAutoFlow, CSSPropertyGrid,
                      *CSSInitialValue::Create(), important);
    AddParsedProperty(CSSPropertyGridAutoColumns, CSSPropertyGrid,
                      *CSSInitialValue::Create(), important);
    AddParsedProperty(CSSPropertyGridAutoRows, CSSPropertyGrid,
                      *CSSInitialValue::Create(), important);
    AddParsedProperty(CSSPropertyGridColumnGap, CSSPropertyGrid,
                      *CSSInitialValue::Create(), important);
    AddParsedProperty(CSSPropertyGridRowGap, CSSPropertyGrid,
                      *CSSInitialValue::Create(), important);
    return true;
  }

  range_ = range_copy;

  CSSValue* auto_columns_value = nullptr;
  CSSValue* auto_rows_value = nullptr;
  CSSValue* template_rows = nullptr;
  CSSValue* template_columns = nullptr;
  CSSValueList* grid_auto_flow = nullptr;
  if (IdentMatches<CSSValueDense, CSSValueAutoFlow>(range_.Peek().Id())) {
    // 2- [ auto-flow && dense? ] <grid-auto-rows>? / <grid-template-columns>
    grid_auto_flow = ConsumeImplicitAutoFlow(
        range_, *CSSIdentifierValue::Create(CSSValueRow));
    if (!grid_auto_flow)
      return false;
    if (ConsumeSlashIncludingWhitespace(range_)) {
      auto_rows_value = CSSInitialValue::Create();
    } else {
      auto_rows_value =
          ConsumeGridTrackList(range_, context_->Mode(), kGridAuto);
      if (!auto_rows_value)
        return false;
      if (!ConsumeSlashIncludingWhitespace(range_))
        return false;
    }
    if (!(template_columns =
              ConsumeGridTemplatesRowsOrColumns(range_, context_->Mode())))
      return false;
    template_rows = CSSInitialValue::Create();
    auto_columns_value = CSSInitialValue::Create();
  } else {
    // 3- <grid-template-rows> / [ auto-flow && dense? ] <grid-auto-columns>?
    template_rows = ConsumeGridTemplatesRowsOrColumns(range_, context_->Mode());
    if (!template_rows)
      return false;
    if (!ConsumeSlashIncludingWhitespace(range_))
      return false;
    grid_auto_flow = ConsumeImplicitAutoFlow(
        range_, *CSSIdentifierValue::Create(CSSValueColumn));
    if (!grid_auto_flow)
      return false;
    if (range_.AtEnd()) {
      auto_columns_value = CSSInitialValue::Create();
    } else {
      auto_columns_value =
          ConsumeGridTrackList(range_, context_->Mode(), kGridAuto);
      if (!auto_columns_value)
        return false;
    }
    template_columns = CSSInitialValue::Create();
    auto_rows_value = CSSInitialValue::Create();
  }

  if (!range_.AtEnd())
    return false;

  // It can only be specified the explicit or the implicit grid properties in a
  // single grid declaration. The sub-properties not specified are set to their
  // initial value, as normal for shorthands.
  AddParsedProperty(CSSPropertyGridTemplateColumns, CSSPropertyGrid,
                    *template_columns, important);
  AddParsedProperty(CSSPropertyGridTemplateRows, CSSPropertyGrid,
                    *template_rows, important);
  AddParsedProperty(CSSPropertyGridTemplateAreas, CSSPropertyGrid,
                    *CSSInitialValue::Create(), important);
  AddParsedProperty(CSSPropertyGridAutoFlow, CSSPropertyGrid, *grid_auto_flow,
                    important);
  AddParsedProperty(CSSPropertyGridAutoColumns, CSSPropertyGrid,
                    *auto_columns_value, important);
  AddParsedProperty(CSSPropertyGridAutoRows, CSSPropertyGrid, *auto_rows_value,
                    important);
  AddParsedProperty(CSSPropertyGridColumnGap, CSSPropertyGrid,
                    *CSSInitialValue::Create(), important);
  AddParsedProperty(CSSPropertyGridRowGap, CSSPropertyGrid,
                    *CSSInitialValue::Create(), important);
  return true;
}

bool CSSPropertyParser::ParseShorthand(CSSPropertyID unresolved_property,
                                       bool important) {
  DCHECK(context_);
  CSSPropertyID property = resolveCSSPropertyID(unresolved_property);

  // Gets the parsing method for our current property from the property API.
  // If it has been implemented, we call this method, otherwise we manually
  // parse this value in the switch statement below. As we implement APIs for
  // other properties, those properties will be taken out of the switch
  // statement.
  const CSSPropertyDescriptor& css_property_desc =
      CSSPropertyDescriptor::Get(property);
  if (css_property_desc.parseShorthand) {
    return css_property_desc.parseShorthand(
        important, range_, *context_, isPropertyAlias(unresolved_property),
        *parsed_properties_);
  }

  switch (property) {
    case CSSPropertyMarker: {
      const CSSValue* marker = ParseSingleValue(CSSPropertyMarkerStart);
      if (!marker || !range_.AtEnd())
        return false;
      AddParsedProperty(CSSPropertyMarkerStart, CSSPropertyMarker, *marker,
                        important);
      AddParsedProperty(CSSPropertyMarkerMid, CSSPropertyMarker, *marker,
                        important);
      AddParsedProperty(CSSPropertyMarkerEnd, CSSPropertyMarker, *marker,
                        important);
      return true;
    }
    case CSSPropertyBorder:
      return ConsumeBorder(important);
    case CSSPropertyBackgroundRepeat:
    case CSSPropertyWebkitMaskRepeat: {
      CSSValue* result_x = nullptr;
      CSSValue* result_y = nullptr;
      bool implicit = false;
      if (!ConsumeRepeatStyle(range_, result_x, result_y, implicit) ||
          !range_.AtEnd())
        return false;
      AddParsedProperty(property == CSSPropertyBackgroundRepeat
                            ? CSSPropertyBackgroundRepeatX
                            : CSSPropertyWebkitMaskRepeatX,
                        property, *result_x, important, implicit);
      AddParsedProperty(property == CSSPropertyBackgroundRepeat
                            ? CSSPropertyBackgroundRepeatY
                            : CSSPropertyWebkitMaskRepeatY,
                        property, *result_y, important, implicit);
      return true;
    }
    case CSSPropertyBackground:
      return ConsumeBackgroundShorthand(backgroundShorthand(), important);
    case CSSPropertyWebkitMask:
      return ConsumeBackgroundShorthand(webkitMaskShorthand(), important);
    case CSSPropertyGridGap: {
      DCHECK(RuntimeEnabledFeatures::CSSGridLayoutEnabled());
      DCHECK_EQ(shorthandForProperty(CSSPropertyGridGap).length(), 2u);
      CSSValue* row_gap = ConsumeLengthOrPercent(range_, context_->Mode(),
                                                 kValueRangeNonNegative);
      CSSValue* column_gap = ConsumeLengthOrPercent(range_, context_->Mode(),
                                                    kValueRangeNonNegative);
      if (!row_gap || !range_.AtEnd())
        return false;
      if (!column_gap)
        column_gap = row_gap;
      AddParsedProperty(CSSPropertyGridRowGap, CSSPropertyGridGap, *row_gap,
                        important);
      AddParsedProperty(CSSPropertyGridColumnGap, CSSPropertyGridGap,
                        *column_gap, important);
      return true;
    }
    case CSSPropertyGridTemplate:
      return ConsumeGridTemplateShorthand(CSSPropertyGridTemplate, important);
    case CSSPropertyGrid:
      return ConsumeGridShorthand(important);
    default:
      return false;
  }
}

}  // namespace blink

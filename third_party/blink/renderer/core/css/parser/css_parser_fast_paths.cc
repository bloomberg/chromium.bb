// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/parser/css_parser_fast_paths.h"

#include "build/build_config.h"
#include "third_party/blink/renderer/core/css/css_color_value.h"
#include "third_party/blink/renderer/core/css/css_function_value.h"
#include "third_party/blink/renderer/core/css/css_identifier_value.h"
#include "third_party/blink/renderer/core/css/css_inherited_value.h"
#include "third_party/blink/renderer/core/css/css_initial_value.h"
#include "third_party/blink/renderer/core/css/css_primitive_value.h"
#include "third_party/blink/renderer/core/css/css_unset_value.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_idioms.h"
#include "third_party/blink/renderer/core/css/parser/css_property_parser.h"
#include "third_party/blink/renderer/core/css/properties/css_property.h"
#include "third_party/blink/renderer/core/css/style_color.h"
#include "third_party/blink/renderer/core/html/parser/html_parser_idioms.h"
#include "third_party/blink/renderer/core/style_property_shorthand.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/wtf/text/string_to_number.h"

namespace blink {

using namespace cssvalue;

static inline bool IsSimpleLengthPropertyID(CSSPropertyID property_id,
                                            bool& accepts_negative_numbers) {
  switch (property_id) {
    case CSSPropertyID::kBlockSize:
    case CSSPropertyID::kInlineSize:
    case CSSPropertyID::kMinBlockSize:
    case CSSPropertyID::kMinInlineSize:
    case CSSPropertyID::kFontSize:
    case CSSPropertyID::kHeight:
    case CSSPropertyID::kWidth:
    case CSSPropertyID::kMinHeight:
    case CSSPropertyID::kMinWidth:
    case CSSPropertyID::kPaddingBottom:
    case CSSPropertyID::kPaddingLeft:
    case CSSPropertyID::kPaddingRight:
    case CSSPropertyID::kPaddingTop:
    case CSSPropertyID::kScrollMarginBlockEnd:
    case CSSPropertyID::kScrollMarginBlockStart:
    case CSSPropertyID::kScrollMarginBottom:
    case CSSPropertyID::kScrollMarginInlineEnd:
    case CSSPropertyID::kScrollMarginInlineStart:
    case CSSPropertyID::kScrollMarginLeft:
    case CSSPropertyID::kScrollMarginRight:
    case CSSPropertyID::kScrollMarginTop:
    case CSSPropertyID::kScrollPaddingBlockEnd:
    case CSSPropertyID::kScrollPaddingBlockStart:
    case CSSPropertyID::kScrollPaddingBottom:
    case CSSPropertyID::kScrollPaddingInlineEnd:
    case CSSPropertyID::kScrollPaddingInlineStart:
    case CSSPropertyID::kScrollPaddingLeft:
    case CSSPropertyID::kScrollPaddingRight:
    case CSSPropertyID::kScrollPaddingTop:
    case CSSPropertyID::kPaddingBlockEnd:
    case CSSPropertyID::kPaddingBlockStart:
    case CSSPropertyID::kPaddingInlineEnd:
    case CSSPropertyID::kPaddingInlineStart:
    case CSSPropertyID::kShapeMargin:
    case CSSPropertyID::kR:
    case CSSPropertyID::kRx:
    case CSSPropertyID::kRy:
      accepts_negative_numbers = false;
      return true;
    case CSSPropertyID::kBottom:
    case CSSPropertyID::kCx:
    case CSSPropertyID::kCy:
    case CSSPropertyID::kLeft:
    case CSSPropertyID::kMarginBottom:
    case CSSPropertyID::kMarginLeft:
    case CSSPropertyID::kMarginRight:
    case CSSPropertyID::kMarginTop:
    case CSSPropertyID::kOffsetDistance:
    case CSSPropertyID::kRight:
    case CSSPropertyID::kTop:
    case CSSPropertyID::kMarginBlockEnd:
    case CSSPropertyID::kMarginBlockStart:
    case CSSPropertyID::kMarginInlineEnd:
    case CSSPropertyID::kMarginInlineStart:
    case CSSPropertyID::kX:
    case CSSPropertyID::kY:
      accepts_negative_numbers = true;
      return true;
    default:
      return false;
  }
}

template <typename CharacterType>
static inline bool ParseSimpleLength(const CharacterType* characters,
                                     unsigned length,
                                     CSSPrimitiveValue::UnitType& unit,
                                     double& number) {
  if (length > 2 && (characters[length - 2] | 0x20) == 'p' &&
      (characters[length - 1] | 0x20) == 'x') {
    length -= 2;
    unit = CSSPrimitiveValue::UnitType::kPixels;
  } else if (length > 1 && characters[length - 1] == '%') {
    length -= 1;
    unit = CSSPrimitiveValue::UnitType::kPercentage;
  }

  // We rely on charactersToDouble for validation as well. The function
  // will set "ok" to "false" if the entire passed-in character range does
  // not represent a double.
  bool ok;
  number = CharactersToDouble(characters, length, &ok);
  if (!ok)
    return false;
  number = clampTo<double>(number, -std::numeric_limits<float>::max(),
                           std::numeric_limits<float>::max());
  return true;
}

static CSSValue* ParseSimpleLengthValue(CSSPropertyID property_id,
                                        const String& string,
                                        CSSParserMode css_parser_mode) {
  DCHECK(!string.IsEmpty());
  bool accepts_negative_numbers = false;

  // In @viewport, width and height are shorthands, not simple length values.
  if (IsCSSViewportParsingEnabledForMode(css_parser_mode) ||
      !IsSimpleLengthPropertyID(property_id, accepts_negative_numbers))
    return nullptr;

  unsigned length = string.length();
  double number;
  CSSPrimitiveValue::UnitType unit = CSSPrimitiveValue::UnitType::kNumber;

  if (string.Is8Bit()) {
    if (!ParseSimpleLength(string.Characters8(), length, unit, number))
      return nullptr;
  } else {
    if (!ParseSimpleLength(string.Characters16(), length, unit, number))
      return nullptr;
  }

  if (unit == CSSPrimitiveValue::UnitType::kNumber) {
    if (css_parser_mode == kSVGAttributeMode)
      unit = CSSPrimitiveValue::UnitType::kUserUnits;
    else if (!number)
      unit = CSSPrimitiveValue::UnitType::kPixels;
    else
      return nullptr;
  }

  if (number < 0 && !accepts_negative_numbers)
    return nullptr;

  return CSSPrimitiveValue::Create(number, unit);
}

static inline bool IsColorPropertyID(CSSPropertyID property_id) {
  switch (property_id) {
    case CSSPropertyID::kCaretColor:
    case CSSPropertyID::kColor:
    case CSSPropertyID::kBackgroundColor:
    case CSSPropertyID::kBorderBottomColor:
    case CSSPropertyID::kBorderLeftColor:
    case CSSPropertyID::kBorderRightColor:
    case CSSPropertyID::kBorderTopColor:
    case CSSPropertyID::kFill:
    case CSSPropertyID::kFloodColor:
    case CSSPropertyID::kLightingColor:
    case CSSPropertyID::kOutlineColor:
    case CSSPropertyID::kStopColor:
    case CSSPropertyID::kStroke:
    case CSSPropertyID::kBorderBlockEndColor:
    case CSSPropertyID::kBorderBlockStartColor:
    case CSSPropertyID::kBorderInlineEndColor:
    case CSSPropertyID::kBorderInlineStartColor:
    case CSSPropertyID::kColumnRuleColor:
    case CSSPropertyID::kWebkitTextEmphasisColor:
    case CSSPropertyID::kWebkitTextFillColor:
    case CSSPropertyID::kWebkitTextStrokeColor:
    case CSSPropertyID::kTextDecorationColor:
      return true;
    default:
      return false;
  }
}

// Returns the number of characters which form a valid double
// and are terminated by the given terminator character
template <typename CharacterType>
static int CheckForValidDouble(const CharacterType* string,
                               const CharacterType* end,
                               const bool terminated_by_space,
                               const char terminator) {
  int length = static_cast<int>(end - string);
  if (length < 1)
    return 0;

  bool decimal_mark_seen = false;
  int processed_length = 0;

  for (int i = 0; i < length; ++i) {
    if (string[i] == terminator ||
        (terminated_by_space && IsHTMLSpace<CharacterType>(string[i]))) {
      processed_length = i;
      break;
    }
    if (!IsASCIIDigit(string[i])) {
      if (!decimal_mark_seen && string[i] == '.')
        decimal_mark_seen = true;
      else
        return 0;
    }
  }

  if (decimal_mark_seen && processed_length == 1)
    return 0;

  return processed_length;
}

// Returns the number of characters consumed for parsing a valid double
// terminated by the given terminator character
template <typename CharacterType>
static int ParseDouble(const CharacterType* string,
                       const CharacterType* end,
                       const char terminator,
                       const bool terminated_by_space,
                       double& value) {
  int length =
      CheckForValidDouble(string, end, terminated_by_space, terminator);
  if (!length)
    return 0;

  int position = 0;
  double local_value = 0;

  // The consumed characters here are guaranteed to be
  // ASCII digits with or without a decimal mark
  for (; position < length; ++position) {
    if (string[position] == '.')
      break;
    local_value = local_value * 10 + string[position] - '0';
  }

  if (++position == length) {
    value = local_value;
    return length;
  }

  double fraction = 0;
  double scale = 1;

  const double kMaxScale = 1000000;
  while (position < length && scale < kMaxScale) {
    fraction = fraction * 10 + string[position++] - '0';
    scale *= 10;
  }

  value = local_value + fraction / scale;
  return length;
}

template <typename CharacterType>
static bool ParseColorNumberOrPercentage(const CharacterType*& string,
                                         const CharacterType* end,
                                         const char terminator,
                                         bool& should_whitespace_terminate,
                                         bool is_first_value,
                                         CSSPrimitiveValue::UnitType& expect,
                                         int& value) {
  const CharacterType* current = string;
  double local_value = 0;
  bool negative = false;
  while (current != end && IsHTMLSpace<CharacterType>(*current))
    current++;
  if (current != end && *current == '-') {
    negative = true;
    current++;
  }
  if (current == end || !IsASCIIDigit(*current))
    return false;
  while (current != end && IsASCIIDigit(*current)) {
    double new_value = local_value * 10 + *current++ - '0';
    if (new_value >= 255) {
      // Clamp values at 255.
      local_value = 255;
      while (current != end && IsASCIIDigit(*current))
        ++current;
      break;
    }
    local_value = new_value;
  }

  if (current == end)
    return false;

  if (expect == CSSPrimitiveValue::UnitType::kNumber && *current == '%')
    return false;

  if (*current == '.') {
    // We already parsed the integral part, try to parse
    // the fraction part.
    double fractional = 0;
    int num_characters_parsed =
        ParseDouble(current, end, '%', false, fractional);
    if (num_characters_parsed) {
      // Number is a percent.
      current += num_characters_parsed;
      if (*current != '%')
        return false;
    } else {
      // Number is a decimal.
      num_characters_parsed =
          ParseDouble(current, end, terminator, true, fractional);
      if (!num_characters_parsed)
        return false;
      current += num_characters_parsed;
    }
    local_value += fractional;
  }

  if (expect == CSSPrimitiveValue::UnitType::kPercentage && *current != '%')
    return false;

  if (*current == '%') {
    expect = CSSPrimitiveValue::UnitType::kPercentage;
    local_value = local_value / 100.0 * 255.0;
    // Clamp values at 255 for percentages over 100%
    if (local_value > 255)
      local_value = 255;
    current++;
  } else {
    expect = CSSPrimitiveValue::UnitType::kNumber;
  }

  while (current != end && IsHTMLSpace<CharacterType>(*current))
    current++;

  if (current == end || *current != terminator) {
    if (!should_whitespace_terminate ||
        !IsHTMLSpace<CharacterType>(*(current - 1))) {
      return false;
    }
  } else if (should_whitespace_terminate && is_first_value) {
    should_whitespace_terminate = false;
  } else if (should_whitespace_terminate) {
    return false;
  }

  if (!should_whitespace_terminate)
    current++;

  // Clamp negative values at zero.
  value = negative ? 0 : static_cast<int>(round(local_value));
  string = current;
  return true;
}

template <typename CharacterType>
static inline bool IsTenthAlpha(const CharacterType* string, const int length) {
  // "0.X"
  if (length == 3 && string[0] == '0' && string[1] == '.' &&
      IsASCIIDigit(string[2]))
    return true;

  // ".X"
  if (length == 2 && string[0] == '.' && IsASCIIDigit(string[1]))
    return true;

  return false;
}

template <typename CharacterType>
static inline bool ParseAlphaValue(const CharacterType*& string,
                                   const CharacterType* end,
                                   const char terminator,
                                   int& value) {
  while (string != end && IsHTMLSpace<CharacterType>(*string))
    string++;

  bool negative = false;

  if (string != end && *string == '-') {
    negative = true;
    string++;
  }

  value = 0;

  size_t length = end - string;
  if (length < 2)
    return false;

  if (string[length - 1] != terminator || !IsASCIIDigit(string[length - 2]))
    return false;

  if (string[0] != '0' && string[0] != '1' && string[0] != '.') {
    if (CheckForValidDouble(string, end, false, terminator)) {
      value = negative ? 0 : 255;
      string = end;
      return true;
    }
    return false;
  }

  if (length == 2 && string[0] != '.') {
    value = !negative && string[0] == '1' ? 255 : 0;
    string = end;
    return true;
  }

  if (IsTenthAlpha(string, length - 1)) {
    // Fast conversions for 0.1 steps of alpha values between 0.0 and 0.9,
    // where 0.1 alpha is value 26 (25.5 rounded) and so on.
    static const int kTenthAlphaValues[] = {0,   26,  51,  77,  102,
                                            128, 153, 179, 204, 230};
    value = negative ? 0 : kTenthAlphaValues[string[length - 2] - '0'];
    string = end;
    return true;
  }

  double alpha = 0;
  if (!ParseDouble(string, end, terminator, false, alpha))
    return false;
  value = negative ? 0 : static_cast<int>(round(std::min(alpha, 1.0) * 255.0));
  string = end;
  return true;
}

template <typename CharacterType>
static inline bool MightBeRGBOrRGBA(const CharacterType* characters,
                                    unsigned length) {
  if (length < 5)
    return false;
  return IsASCIIAlphaCaselessEqual(characters[0], 'r') &&
         IsASCIIAlphaCaselessEqual(characters[1], 'g') &&
         IsASCIIAlphaCaselessEqual(characters[2], 'b') &&
         (characters[3] == '(' ||
          (IsASCIIAlphaCaselessEqual(characters[3], 'a') &&
           characters[4] == '('));
}

template <typename CharacterType>
static bool FastParseColorInternal(RGBA32& rgb,
                                   const CharacterType* characters,
                                   unsigned length,
                                   bool quirks_mode) {
  CSSPrimitiveValue::UnitType expect = CSSPrimitiveValue::UnitType::kUnknown;

  if (length >= 4 && characters[0] == '#')
    return Color::ParseHexColor(characters + 1, length - 1, rgb);

  if (quirks_mode && (length == 3 || length == 6)) {
    if (Color::ParseHexColor(characters, length, rgb))
      return true;
  }

  // rgb() and rgba() have the same syntax
  if (MightBeRGBOrRGBA(characters, length)) {
    int length_to_add = IsASCIIAlphaCaselessEqual(characters[3], 'a') ? 5 : 4;
    const CharacterType* current = characters + length_to_add;
    const CharacterType* end = characters + length;
    int red;
    int green;
    int blue;
    int alpha;
    bool should_have_alpha = false;
    bool should_whitespace_terminate = true;
    bool no_whitespace_check = false;

    if (!ParseColorNumberOrPercentage(current, end, ',',
                                      should_whitespace_terminate,
                                      true /* is_first_value */, expect, red))
      return false;
    if (!ParseColorNumberOrPercentage(
            current, end, ',', should_whitespace_terminate,
            false /* is_first_value */, expect, green))
      return false;
    if (!ParseColorNumberOrPercentage(current, end, ',', no_whitespace_check,
                                      false /* is_first_value */, expect,
                                      blue)) {
      // Might have slash as separator
      if (ParseColorNumberOrPercentage(current, end, '/', no_whitespace_check,
                                       false /* is_first_value */, expect,
                                       blue)) {
        if (!should_whitespace_terminate)
          return false;
        should_have_alpha = true;
      }
      // Might not have alpha
      else if (!ParseColorNumberOrPercentage(
                   current, end, ')', no_whitespace_check,
                   false /* is_first_value */, expect, blue))
        return false;
    } else {
      if (should_whitespace_terminate)
        return false;
      should_have_alpha = true;
    }

    if (should_have_alpha) {
      if (!ParseAlphaValue(current, end, ')', alpha))
        return false;
      if (current != end)
        return false;
      rgb = MakeRGBA(red, green, blue, alpha);
    } else {
      if (current != end)
        return false;
      rgb = MakeRGB(red, green, blue);
    }
    return true;
  }

  return false;
}

CSSValue* CSSParserFastPaths::ParseColor(const String& string,
                                         CSSParserMode parser_mode) {
  DCHECK(!string.IsEmpty());
  CSSValueID value_id = CssValueKeywordID(string);
  if (StyleColor::IsColorKeyword(value_id)) {
    if (!isValueAllowedInMode(value_id, parser_mode))
      return nullptr;
    return CSSIdentifierValue::Create(value_id);
  }

  RGBA32 color;
  bool quirks_mode = IsQuirksModeBehavior(parser_mode);

  // Fast path for hex colors and rgb()/rgba() colors
  bool parse_result;
  if (string.Is8Bit())
    parse_result = FastParseColorInternal(color, string.Characters8(),
                                          string.length(), quirks_mode);
  else
    parse_result = FastParseColorInternal(color, string.Characters16(),
                                          string.length(), quirks_mode);
  if (!parse_result)
    return nullptr;
  return CSSColorValue::Create(color);
}

bool CSSParserFastPaths::IsValidKeywordPropertyAndValue(
    CSSPropertyID property_id,
    CSSValueID value_id,
    CSSParserMode parser_mode) {
  if (value_id == CSSValueInvalid ||
      !isValueAllowedInMode(value_id, parser_mode))
    return false;

  // For range checks, enum ordering is defined by CSSValueKeywords.in.
  switch (property_id) {
    case CSSPropertyID::kAlignmentBaseline:
      return value_id == CSSValueAuto || value_id == CSSValueAlphabetic ||
             value_id == CSSValueBaseline || value_id == CSSValueMiddle ||
             (value_id >= CSSValueBeforeEdge &&
              value_id <= CSSValueMathematical);
    case CSSPropertyID::kAll:
      return false;  // Only accepts css-wide keywords
    case CSSPropertyID::kBackgroundRepeatX:
    case CSSPropertyID::kBackgroundRepeatY:
      return value_id == CSSValueRepeat || value_id == CSSValueNoRepeat;
    case CSSPropertyID::kBorderCollapse:
      return value_id == CSSValueCollapse || value_id == CSSValueSeparate;
    case CSSPropertyID::kBorderTopStyle:
    case CSSPropertyID::kBorderRightStyle:
    case CSSPropertyID::kBorderBottomStyle:
    case CSSPropertyID::kBorderLeftStyle:
    case CSSPropertyID::kBorderBlockEndStyle:
    case CSSPropertyID::kBorderBlockStartStyle:
    case CSSPropertyID::kBorderInlineEndStyle:
    case CSSPropertyID::kBorderInlineStartStyle:
    case CSSPropertyID::kColumnRuleStyle:
      return value_id >= CSSValueNone && value_id <= CSSValueDouble;
    case CSSPropertyID::kBoxSizing:
      return value_id == CSSValueBorderBox || value_id == CSSValueContentBox;
    case CSSPropertyID::kBufferedRendering:
      return value_id == CSSValueAuto || value_id == CSSValueDynamic ||
             value_id == CSSValueStatic;
    case CSSPropertyID::kCaptionSide:
      return value_id == CSSValueTop || value_id == CSSValueBottom;
    case CSSPropertyID::kClear:
      return value_id == CSSValueNone || value_id == CSSValueLeft ||
             value_id == CSSValueRight || value_id == CSSValueBoth ||
             (RuntimeEnabledFeatures::CSSLogicalEnabled() &&
              (value_id == CSSValueInlineStart ||
               value_id == CSSValueInlineEnd));
    case CSSPropertyID::kClipRule:
    case CSSPropertyID::kFillRule:
      return value_id == CSSValueNonzero || value_id == CSSValueEvenodd;
    case CSSPropertyID::kColorInterpolation:
    case CSSPropertyID::kColorInterpolationFilters:
      return value_id == CSSValueAuto || value_id == CSSValueSRGB ||
             value_id == CSSValueLinearrgb;
    case CSSPropertyID::kColorRendering:
      return value_id == CSSValueAuto || value_id == CSSValueOptimizespeed ||
             value_id == CSSValueOptimizequality;
    case CSSPropertyID::kDirection:
      return value_id == CSSValueLtr || value_id == CSSValueRtl;
    case CSSPropertyID::kDisplay:
      return (value_id >= CSSValueInline && value_id <= CSSValueInlineFlex) ||
             value_id == CSSValueWebkitFlex ||
             value_id == CSSValueWebkitInlineFlex || value_id == CSSValueNone ||
             value_id == CSSValueGrid || value_id == CSSValueInlineGrid ||
             value_id == CSSValueContents;
    case CSSPropertyID::kDominantBaseline:
      return value_id == CSSValueAuto || value_id == CSSValueAlphabetic ||
             value_id == CSSValueMiddle ||
             (value_id >= CSSValueUseScript && value_id <= CSSValueResetSize) ||
             (value_id >= CSSValueCentral && value_id <= CSSValueMathematical);
    case CSSPropertyID::kEmptyCells:
      return value_id == CSSValueShow || value_id == CSSValueHide;
    case CSSPropertyID::kFloat:
      return value_id == CSSValueLeft || value_id == CSSValueRight ||
             (RuntimeEnabledFeatures::CSSLogicalEnabled() &&
              (value_id == CSSValueInlineStart ||
               value_id == CSSValueInlineEnd)) ||
             value_id == CSSValueNone;
    case CSSPropertyID::kImageRendering:
      return value_id == CSSValueAuto ||
             value_id == CSSValueWebkitOptimizeContrast ||
             value_id == CSSValuePixelated;
    case CSSPropertyID::kIsolation:
      return value_id == CSSValueAuto || value_id == CSSValueIsolate;
    case CSSPropertyID::kListStylePosition:
      return value_id == CSSValueInside || value_id == CSSValueOutside;
    case CSSPropertyID::kListStyleType:
      return (value_id >= CSSValueDisc && value_id <= CSSValueKatakanaIroha) ||
             value_id == CSSValueNone;
    case CSSPropertyID::kMaskType:
      return value_id == CSSValueLuminance || value_id == CSSValueAlpha;
    case CSSPropertyID::kObjectFit:
      return value_id == CSSValueFill || value_id == CSSValueContain ||
             value_id == CSSValueCover || value_id == CSSValueNone ||
             value_id == CSSValueScaleDown;
    case CSSPropertyID::kOutlineStyle:
      return value_id == CSSValueAuto || value_id == CSSValueNone ||
             (value_id >= CSSValueInset && value_id <= CSSValueDouble);
    case CSSPropertyID::kOverflowAnchor:
      return value_id == CSSValueVisible || value_id == CSSValueNone ||
             value_id == CSSValueAuto;
    case CSSPropertyID::kOverflowWrap:
      return value_id == CSSValueNormal || value_id == CSSValueBreakWord;
    case CSSPropertyID::kOverflowX:
      return value_id == CSSValueVisible || value_id == CSSValueHidden ||
             value_id == CSSValueScroll || value_id == CSSValueAuto ||
             value_id == CSSValueOverlay;
    case CSSPropertyID::kOverflowY:
      return value_id == CSSValueVisible || value_id == CSSValueHidden ||
             value_id == CSSValueScroll || value_id == CSSValueAuto ||
             value_id == CSSValueOverlay || value_id == CSSValueWebkitPagedX ||
             value_id == CSSValueWebkitPagedY;
    case CSSPropertyID::kBreakAfter:
    case CSSPropertyID::kBreakBefore:
      return value_id == CSSValueAuto || value_id == CSSValueAvoid ||
             value_id == CSSValueAvoidPage || value_id == CSSValuePage ||
             value_id == CSSValueLeft || value_id == CSSValueRight ||
             value_id == CSSValueRecto || value_id == CSSValueVerso ||
             value_id == CSSValueAvoidColumn || value_id == CSSValueColumn;
    case CSSPropertyID::kBreakInside:
      return value_id == CSSValueAuto || value_id == CSSValueAvoid ||
             value_id == CSSValueAvoidPage || value_id == CSSValueAvoidColumn;
    case CSSPropertyID::kPointerEvents:
      return value_id == CSSValueVisible || value_id == CSSValueNone ||
             value_id == CSSValueAll || value_id == CSSValueAuto ||
             (value_id >= CSSValueVisiblepainted &&
              value_id <= CSSValueBoundingBox);
    case CSSPropertyID::kPosition:
      return value_id == CSSValueStatic || value_id == CSSValueRelative ||
             value_id == CSSValueAbsolute || value_id == CSSValueFixed ||
             value_id == CSSValueSticky;
    case CSSPropertyID::kResize:
      return value_id == CSSValueNone || value_id == CSSValueBoth ||
             value_id == CSSValueHorizontal || value_id == CSSValueVertical ||
             (RuntimeEnabledFeatures::CSSLogicalEnabled() &&
              (value_id == CSSValueBlock || value_id == CSSValueInline)) ||
             value_id == CSSValueAuto;
    case CSSPropertyID::kScrollBehavior:
      return value_id == CSSValueAuto || value_id == CSSValueSmooth;
    case CSSPropertyID::kShapeRendering:
      return value_id == CSSValueAuto || value_id == CSSValueOptimizespeed ||
             value_id == CSSValueCrispedges ||
             value_id == CSSValueGeometricprecision;
    case CSSPropertyID::kSpeak:
      return value_id == CSSValueNone || value_id == CSSValueNormal ||
             value_id == CSSValueSpellOut || value_id == CSSValueDigits ||
             value_id == CSSValueLiteralPunctuation ||
             value_id == CSSValueNoPunctuation;
    case CSSPropertyID::kStrokeLinejoin:
      return value_id == CSSValueMiter || value_id == CSSValueRound ||
             value_id == CSSValueBevel;
    case CSSPropertyID::kStrokeLinecap:
      return value_id == CSSValueButt || value_id == CSSValueRound ||
             value_id == CSSValueSquare;
    case CSSPropertyID::kTableLayout:
      return value_id == CSSValueAuto || value_id == CSSValueFixed;
    case CSSPropertyID::kTextAlign:
      return (value_id >= CSSValueWebkitAuto &&
              value_id <= CSSValueInternalCenter) ||
             value_id == CSSValueStart || value_id == CSSValueEnd;
    case CSSPropertyID::kTextAlignLast:
      return (value_id >= CSSValueLeft && value_id <= CSSValueJustify) ||
             value_id == CSSValueStart || value_id == CSSValueEnd ||
             value_id == CSSValueAuto;
    case CSSPropertyID::kTextAnchor:
      return value_id == CSSValueStart || value_id == CSSValueMiddle ||
             value_id == CSSValueEnd;
    case CSSPropertyID::kTextCombineUpright:
      return value_id == CSSValueNone || value_id == CSSValueAll;
    case CSSPropertyID::kTextDecorationStyle:
      return value_id == CSSValueSolid || value_id == CSSValueDouble ||
             value_id == CSSValueDotted || value_id == CSSValueDashed ||
             value_id == CSSValueWavy;
    case CSSPropertyID::kTextDecorationSkipInk:
      return value_id == CSSValueAuto || value_id == CSSValueNone;
    case CSSPropertyID::kTextJustify:
      DCHECK(RuntimeEnabledFeatures::CSS3TextEnabled());
      return value_id == CSSValueInterWord || value_id == CSSValueDistribute ||
             value_id == CSSValueAuto || value_id == CSSValueNone;
    case CSSPropertyID::kTextOrientation:
      return value_id == CSSValueMixed || value_id == CSSValueUpright ||
             value_id == CSSValueSideways || value_id == CSSValueSidewaysRight;
    case CSSPropertyID::kWebkitTextOrientation:
      return value_id == CSSValueSideways ||
             value_id == CSSValueSidewaysRight ||
             value_id == CSSValueVerticalRight || value_id == CSSValueUpright;
    case CSSPropertyID::kTextOverflow:
      return value_id == CSSValueClip || value_id == CSSValueEllipsis;
    case CSSPropertyID::kTextRendering:
      return value_id == CSSValueAuto || value_id == CSSValueOptimizespeed ||
             value_id == CSSValueOptimizelegibility ||
             value_id == CSSValueGeometricprecision;
    case CSSPropertyID::kTextTransform:  // capitalize | uppercase | lowercase |
                                         // none
      return (value_id >= CSSValueCapitalize &&
              value_id <= CSSValueLowercase) ||
             value_id == CSSValueNone;
    case CSSPropertyID::kUnicodeBidi:
      return value_id == CSSValueNormal || value_id == CSSValueEmbed ||
             value_id == CSSValueBidiOverride ||
             value_id == CSSValueWebkitIsolate ||
             value_id == CSSValueWebkitIsolateOverride ||
             value_id == CSSValueWebkitPlaintext ||
             value_id == CSSValueIsolate ||
             value_id == CSSValueIsolateOverride ||
             value_id == CSSValuePlaintext;
    case CSSPropertyID::kVectorEffect:
      return value_id == CSSValueNone || value_id == CSSValueNonScalingStroke;
    case CSSPropertyID::kVisibility:
      return value_id == CSSValueVisible || value_id == CSSValueHidden ||
             value_id == CSSValueCollapse;
    case CSSPropertyID::kWebkitAppRegion:
      return (value_id >= CSSValueDrag && value_id <= CSSValueNoDrag) ||
             value_id == CSSValueNone;
    case CSSPropertyID::kWebkitAppearance:
      return (value_id >= CSSValueCheckbox && value_id <= CSSValueTextarea) ||
             value_id == CSSValueNone;
    case CSSPropertyID::kBackfaceVisibility:
      return value_id == CSSValueVisible || value_id == CSSValueHidden;
    case CSSPropertyID::kMixBlendMode:
      return value_id == CSSValueNormal || value_id == CSSValueMultiply ||
             value_id == CSSValueScreen || value_id == CSSValueOverlay ||
             value_id == CSSValueDarken || value_id == CSSValueLighten ||
             value_id == CSSValueColorDodge || value_id == CSSValueColorBurn ||
             value_id == CSSValueHardLight || value_id == CSSValueSoftLight ||
             value_id == CSSValueDifference || value_id == CSSValueExclusion ||
             value_id == CSSValueHue || value_id == CSSValueSaturation ||
             value_id == CSSValueColor || value_id == CSSValueLuminosity;
    case CSSPropertyID::kWebkitBoxAlign:
      return value_id == CSSValueStretch || value_id == CSSValueStart ||
             value_id == CSSValueEnd || value_id == CSSValueCenter ||
             value_id == CSSValueBaseline;
    case CSSPropertyID::kWebkitBoxDecorationBreak:
      return value_id == CSSValueClone || value_id == CSSValueSlice;
    case CSSPropertyID::kWebkitBoxDirection:
      return value_id == CSSValueNormal || value_id == CSSValueReverse;
    case CSSPropertyID::kWebkitBoxOrient:
      return value_id == CSSValueHorizontal || value_id == CSSValueVertical ||
             value_id == CSSValueInlineAxis || value_id == CSSValueBlockAxis;
    case CSSPropertyID::kWebkitBoxPack:
      return value_id == CSSValueStart || value_id == CSSValueEnd ||
             value_id == CSSValueCenter || value_id == CSSValueJustify;
    case CSSPropertyID::kColumnFill:
      return value_id == CSSValueAuto || value_id == CSSValueBalance;
    case CSSPropertyID::kAlignContent:
      // FIXME: Per CSS alignment, this property should accept an optional
      // <overflow-position>. We should share this parsing code with
      // 'justify-self'.
      return value_id == CSSValueFlexStart || value_id == CSSValueFlexEnd ||
             value_id == CSSValueCenter || value_id == CSSValueSpaceBetween ||
             value_id == CSSValueSpaceAround || value_id == CSSValueStretch;
    case CSSPropertyID::kAlignItems:
      // FIXME: Per CSS alignment, this property should accept the same
      // arguments as 'justify-self' so we should share its parsing code.
      return value_id == CSSValueFlexStart || value_id == CSSValueFlexEnd ||
             value_id == CSSValueCenter || value_id == CSSValueBaseline ||
             value_id == CSSValueStretch;
    case CSSPropertyID::kAlignSelf:
      // FIXME: Per CSS alignment, this property should accept the same
      // arguments as 'justify-self' so we should share its parsing code.
      return value_id == CSSValueAuto || value_id == CSSValueFlexStart ||
             value_id == CSSValueFlexEnd || value_id == CSSValueCenter ||
             value_id == CSSValueBaseline || value_id == CSSValueStretch;
    case CSSPropertyID::kFlexDirection:
      return value_id == CSSValueRow || value_id == CSSValueRowReverse ||
             value_id == CSSValueColumn || value_id == CSSValueColumnReverse;
    case CSSPropertyID::kFlexWrap:
      return value_id == CSSValueNowrap || value_id == CSSValueWrap ||
             value_id == CSSValueWrapReverse;
    case CSSPropertyID::kHyphens:
#if defined(OS_ANDROID) || defined(OS_MACOSX)
      return value_id == CSSValueAuto || value_id == CSSValueNone ||
             value_id == CSSValueManual;
#else
      return value_id == CSSValueNone || value_id == CSSValueManual;
#endif
    case CSSPropertyID::kJustifyContent:
      // FIXME: Per CSS alignment, this property should accept an optional
      // <overflow-position>. We should share this parsing code with
      // 'justify-self'.
      return value_id == CSSValueFlexStart || value_id == CSSValueFlexEnd ||
             value_id == CSSValueCenter || value_id == CSSValueSpaceBetween ||
             value_id == CSSValueSpaceAround;
    case CSSPropertyID::kFontKerning:
      return value_id == CSSValueAuto || value_id == CSSValueNormal ||
             value_id == CSSValueNone;
    case CSSPropertyID::kWebkitFontSmoothing:
      return value_id == CSSValueAuto || value_id == CSSValueNone ||
             value_id == CSSValueAntialiased ||
             value_id == CSSValueSubpixelAntialiased;
    case CSSPropertyID::kLineBreak:
      return value_id == CSSValueAuto || value_id == CSSValueLoose ||
             value_id == CSSValueNormal || value_id == CSSValueStrict;
    case CSSPropertyID::kWebkitLineBreak:
      return value_id == CSSValueAuto || value_id == CSSValueLoose ||
             value_id == CSSValueNormal || value_id == CSSValueStrict ||
             value_id == CSSValueAfterWhiteSpace;
    case CSSPropertyID::kWebkitMarginAfterCollapse:
    case CSSPropertyID::kWebkitMarginBeforeCollapse:
    case CSSPropertyID::kWebkitMarginBottomCollapse:
    case CSSPropertyID::kWebkitMarginTopCollapse:
      return value_id == CSSValueCollapse || value_id == CSSValueSeparate ||
             value_id == CSSValueDiscard;
    case CSSPropertyID::kWebkitPrintColorAdjust:
      return value_id == CSSValueExact || value_id == CSSValueEconomy;
    case CSSPropertyID::kWebkitRtlOrdering:
      return value_id == CSSValueLogical || value_id == CSSValueVisual;
    case CSSPropertyID::kWebkitRubyPosition:
      return value_id == CSSValueBefore || value_id == CSSValueAfter;
    case CSSPropertyID::kWebkitTextCombine:
      return value_id == CSSValueNone || value_id == CSSValueHorizontal;
    case CSSPropertyID::kWebkitTextSecurity:
      return value_id == CSSValueDisc || value_id == CSSValueCircle ||
             value_id == CSSValueSquare || value_id == CSSValueNone;
    case CSSPropertyID::kTransformBox:
      return value_id == CSSValueFillBox || value_id == CSSValueViewBox;
    case CSSPropertyID::kTransformStyle:
      return value_id == CSSValueFlat || value_id == CSSValuePreserve3d;
    case CSSPropertyID::kWebkitUserDrag:
      return value_id == CSSValueAuto || value_id == CSSValueNone ||
             value_id == CSSValueElement;
    case CSSPropertyID::kWebkitUserModify:
      return value_id == CSSValueReadOnly || value_id == CSSValueReadWrite ||
             value_id == CSSValueReadWritePlaintextOnly;
    case CSSPropertyID::kUserSelect:
      return value_id == CSSValueAuto || value_id == CSSValueNone ||
             value_id == CSSValueText || value_id == CSSValueAll;
    case CSSPropertyID::kWebkitWritingMode:
      return value_id >= CSSValueHorizontalTb && value_id <= CSSValueVerticalLr;
    case CSSPropertyID::kWritingMode:
      return value_id == CSSValueHorizontalTb ||
             value_id == CSSValueVerticalRl || value_id == CSSValueVerticalLr ||
             value_id == CSSValueLrTb || value_id == CSSValueRlTb ||
             value_id == CSSValueTbRl || value_id == CSSValueLr ||
             value_id == CSSValueRl || value_id == CSSValueTb;
    case CSSPropertyID::kWhiteSpace:
      return value_id == CSSValueNormal || value_id == CSSValuePre ||
             value_id == CSSValuePreWrap || value_id == CSSValuePreLine ||
             value_id == CSSValueNowrap ||
             (RuntimeEnabledFeatures::CSS3TextBreakSpacesEnabled() &&
              value_id == CSSValueBreakSpaces);
    case CSSPropertyID::kWordBreak:
      return value_id == CSSValueNormal || value_id == CSSValueBreakAll ||
             value_id == CSSValueKeepAll || value_id == CSSValueBreakWord;
    case CSSPropertyID::kScrollSnapStop:
      return value_id == CSSValueNormal || value_id == CSSValueAlways;
    case CSSPropertyID::kOverscrollBehaviorX:
      return value_id == CSSValueAuto || value_id == CSSValueContain ||
             value_id == CSSValueNone;
    case CSSPropertyID::kOverscrollBehaviorY:
      return value_id == CSSValueAuto || value_id == CSSValueContain ||
             value_id == CSSValueNone;
    default:
      NOTREACHED();
      return false;
  }
}

bool CSSParserFastPaths::IsKeywordPropertyID(CSSPropertyID property_id) {
  switch (property_id) {
    case CSSPropertyID::kAlignmentBaseline:
    case CSSPropertyID::kAll:
    case CSSPropertyID::kMixBlendMode:
    case CSSPropertyID::kIsolation:
    case CSSPropertyID::kBackgroundRepeatX:
    case CSSPropertyID::kBackgroundRepeatY:
    case CSSPropertyID::kBorderBottomStyle:
    case CSSPropertyID::kBorderCollapse:
    case CSSPropertyID::kBorderLeftStyle:
    case CSSPropertyID::kBorderRightStyle:
    case CSSPropertyID::kBorderTopStyle:
    case CSSPropertyID::kBoxSizing:
    case CSSPropertyID::kBufferedRendering:
    case CSSPropertyID::kCaptionSide:
    case CSSPropertyID::kClear:
    case CSSPropertyID::kClipRule:
    case CSSPropertyID::kColorInterpolation:
    case CSSPropertyID::kColorInterpolationFilters:
    case CSSPropertyID::kColorRendering:
    case CSSPropertyID::kDirection:
    case CSSPropertyID::kDisplay:
    case CSSPropertyID::kDominantBaseline:
    case CSSPropertyID::kEmptyCells:
    case CSSPropertyID::kFillRule:
    case CSSPropertyID::kFloat:
    case CSSPropertyID::kHyphens:
    case CSSPropertyID::kImageRendering:
    case CSSPropertyID::kListStylePosition:
    case CSSPropertyID::kListStyleType:
    case CSSPropertyID::kMaskType:
    case CSSPropertyID::kObjectFit:
    case CSSPropertyID::kOutlineStyle:
    case CSSPropertyID::kOverflowAnchor:
    case CSSPropertyID::kOverflowWrap:
    case CSSPropertyID::kOverflowX:
    case CSSPropertyID::kOverflowY:
    case CSSPropertyID::kBreakAfter:
    case CSSPropertyID::kBreakBefore:
    case CSSPropertyID::kBreakInside:
    case CSSPropertyID::kPointerEvents:
    case CSSPropertyID::kPosition:
    case CSSPropertyID::kResize:
    case CSSPropertyID::kScrollBehavior:
    case CSSPropertyID::kOverscrollBehaviorX:
    case CSSPropertyID::kOverscrollBehaviorY:
    case CSSPropertyID::kShapeRendering:
    case CSSPropertyID::kSpeak:
    case CSSPropertyID::kStrokeLinecap:
    case CSSPropertyID::kStrokeLinejoin:
    case CSSPropertyID::kTableLayout:
    case CSSPropertyID::kTextAlign:
    case CSSPropertyID::kTextAlignLast:
    case CSSPropertyID::kTextAnchor:
    case CSSPropertyID::kTextCombineUpright:
    case CSSPropertyID::kTextDecorationStyle:
    case CSSPropertyID::kTextDecorationSkipInk:
    case CSSPropertyID::kTextJustify:
    case CSSPropertyID::kTextOrientation:
    case CSSPropertyID::kWebkitTextOrientation:
    case CSSPropertyID::kTextOverflow:
    case CSSPropertyID::kTextRendering:
    case CSSPropertyID::kTextTransform:
    case CSSPropertyID::kUnicodeBidi:
    case CSSPropertyID::kVectorEffect:
    case CSSPropertyID::kVisibility:
    case CSSPropertyID::kWebkitAppRegion:
    case CSSPropertyID::kWebkitAppearance:
    case CSSPropertyID::kBackfaceVisibility:
    case CSSPropertyID::kBorderBlockEndStyle:
    case CSSPropertyID::kBorderBlockStartStyle:
    case CSSPropertyID::kBorderInlineEndStyle:
    case CSSPropertyID::kBorderInlineStartStyle:
    case CSSPropertyID::kWebkitBoxAlign:
    case CSSPropertyID::kWebkitBoxDecorationBreak:
    case CSSPropertyID::kWebkitBoxDirection:
    case CSSPropertyID::kWebkitBoxOrient:
    case CSSPropertyID::kWebkitBoxPack:
    case CSSPropertyID::kColumnFill:
    case CSSPropertyID::kColumnRuleStyle:
    case CSSPropertyID::kFlexDirection:
    case CSSPropertyID::kFlexWrap:
    case CSSPropertyID::kFontKerning:
    case CSSPropertyID::kWebkitFontSmoothing:
    case CSSPropertyID::kLineBreak:
    case CSSPropertyID::kWebkitLineBreak:
    case CSSPropertyID::kWebkitMarginAfterCollapse:
    case CSSPropertyID::kWebkitMarginBeforeCollapse:
    case CSSPropertyID::kWebkitMarginBottomCollapse:
    case CSSPropertyID::kWebkitMarginTopCollapse:
    case CSSPropertyID::kWebkitPrintColorAdjust:
    case CSSPropertyID::kWebkitRtlOrdering:
    case CSSPropertyID::kWebkitRubyPosition:
    case CSSPropertyID::kWebkitTextCombine:
    case CSSPropertyID::kWebkitTextSecurity:
    case CSSPropertyID::kTransformBox:
    case CSSPropertyID::kTransformStyle:
    case CSSPropertyID::kWebkitUserDrag:
    case CSSPropertyID::kWebkitUserModify:
    case CSSPropertyID::kUserSelect:
    case CSSPropertyID::kWebkitWritingMode:
    case CSSPropertyID::kWhiteSpace:
    case CSSPropertyID::kWordBreak:
    case CSSPropertyID::kWritingMode:
    case CSSPropertyID::kScrollSnapStop:
      return true;
    default:
      return false;
  }
}

bool CSSParserFastPaths::IsPartialKeywordPropertyID(CSSPropertyID property_id) {
  switch (property_id) {
    case CSSPropertyID::kDisplay:
      return true;
    default:
      return false;
  }
}

static CSSValue* ParseKeywordValue(CSSPropertyID property_id,
                                   const String& string,
                                   CSSParserMode parser_mode) {
  DCHECK(!string.IsEmpty());

  if (!CSSParserFastPaths::IsKeywordPropertyID(property_id)) {
    // All properties accept the values of "initial," "inherit" and "unset".
    if (!EqualIgnoringASCIICase(string, "initial") &&
        !EqualIgnoringASCIICase(string, "inherit") &&
        !EqualIgnoringASCIICase(string, "unset"))
      return nullptr;

    // Parse initial/inherit/unset shorthands using the CSSPropertyParser.
    if (shorthandForProperty(property_id).length())
      return nullptr;

    // Descriptors do not support css wide keywords.
    if (!CSSProperty::Get(property_id).IsProperty())
      return nullptr;
  }

  CSSValueID value_id = CssValueKeywordID(string);

  if (!value_id)
    return nullptr;

  if (value_id == CSSValueInherit)
    return CSSInheritedValue::Create();
  if (value_id == CSSValueInitial)
    return CSSInitialValue::Create();
  if (value_id == CSSValueUnset)
    return cssvalue::CSSUnsetValue::Create();
  if (CSSParserFastPaths::IsValidKeywordPropertyAndValue(property_id, value_id,
                                                         parser_mode))
    return CSSIdentifierValue::Create(value_id);
  return nullptr;
}

template <typename CharType>
static bool ParseTransformTranslateArguments(
    CharType*& pos,
    CharType* end,
    unsigned expected_count,
    CSSFunctionValue* transform_value) {
  while (expected_count) {
    wtf_size_t delimiter = WTF::Find(pos, static_cast<wtf_size_t>(end - pos),
                                     expected_count == 1 ? ')' : ',');
    if (delimiter == kNotFound)
      return false;
    unsigned argument_length = static_cast<unsigned>(delimiter);
    CSSPrimitiveValue::UnitType unit = CSSPrimitiveValue::UnitType::kNumber;
    double number;
    if (!ParseSimpleLength(pos, argument_length, unit, number))
      return false;
    if (unit != CSSPrimitiveValue::UnitType::kPixels &&
        (number || unit != CSSPrimitiveValue::UnitType::kNumber))
      return false;
    transform_value->Append(*CSSPrimitiveValue::Create(
        number, CSSPrimitiveValue::UnitType::kPixels));
    pos += argument_length + 1;
    --expected_count;
  }
  return true;
}

template <typename CharType>
static bool ParseTransformNumberArguments(CharType*& pos,
                                          CharType* end,
                                          unsigned expected_count,
                                          CSSFunctionValue* transform_value) {
  while (expected_count) {
    wtf_size_t delimiter = WTF::Find(pos, static_cast<wtf_size_t>(end - pos),
                                     expected_count == 1 ? ')' : ',');
    if (delimiter == kNotFound)
      return false;
    unsigned argument_length = static_cast<unsigned>(delimiter);
    bool ok;
    double number = CharactersToDouble(pos, argument_length, &ok);
    if (!ok)
      return false;
    transform_value->Append(*CSSPrimitiveValue::Create(
        number, CSSPrimitiveValue::UnitType::kNumber));
    pos += argument_length + 1;
    --expected_count;
  }
  return true;
}

static const int kShortestValidTransformStringLength = 12;

template <typename CharType>
static CSSFunctionValue* ParseSimpleTransformValue(CharType*& pos,
                                                   CharType* end) {
  if (end - pos < kShortestValidTransformStringLength)
    return nullptr;

  const bool is_translate =
      ToASCIILower(pos[0]) == 't' && ToASCIILower(pos[1]) == 'r' &&
      ToASCIILower(pos[2]) == 'a' && ToASCIILower(pos[3]) == 'n' &&
      ToASCIILower(pos[4]) == 's' && ToASCIILower(pos[5]) == 'l' &&
      ToASCIILower(pos[6]) == 'a' && ToASCIILower(pos[7]) == 't' &&
      ToASCIILower(pos[8]) == 'e';

  if (is_translate) {
    CSSValueID transform_type;
    unsigned expected_argument_count = 1;
    unsigned argument_start = 11;
    CharType c9 = ToASCIILower(pos[9]);
    if (c9 == 'x' && pos[10] == '(') {
      transform_type = CSSValueTranslateX;
    } else if (c9 == 'y' && pos[10] == '(') {
      transform_type = CSSValueTranslateY;
    } else if (c9 == 'z' && pos[10] == '(') {
      transform_type = CSSValueTranslateZ;
    } else if (c9 == '(') {
      transform_type = CSSValueTranslate;
      expected_argument_count = 2;
      argument_start = 10;
    } else if (c9 == '3' && ToASCIILower(pos[10]) == 'd' && pos[11] == '(') {
      transform_type = CSSValueTranslate3d;
      expected_argument_count = 3;
      argument_start = 12;
    } else {
      return nullptr;
    }
    pos += argument_start;
    CSSFunctionValue* transform_value =
        CSSFunctionValue::Create(transform_type);
    if (!ParseTransformTranslateArguments(pos, end, expected_argument_count,
                                          transform_value))
      return nullptr;
    return transform_value;
  }

  const bool is_matrix3d =
      ToASCIILower(pos[0]) == 'm' && ToASCIILower(pos[1]) == 'a' &&
      ToASCIILower(pos[2]) == 't' && ToASCIILower(pos[3]) == 'r' &&
      ToASCIILower(pos[4]) == 'i' && ToASCIILower(pos[5]) == 'x' &&
      pos[6] == '3' && ToASCIILower(pos[7]) == 'd' && pos[8] == '(';

  if (is_matrix3d) {
    pos += 9;
    CSSFunctionValue* transform_value =
        CSSFunctionValue::Create(CSSValueMatrix3d);
    if (!ParseTransformNumberArguments(pos, end, 16, transform_value))
      return nullptr;
    return transform_value;
  }

  const bool is_scale3d =
      ToASCIILower(pos[0]) == 's' && ToASCIILower(pos[1]) == 'c' &&
      ToASCIILower(pos[2]) == 'a' && ToASCIILower(pos[3]) == 'l' &&
      ToASCIILower(pos[4]) == 'e' && pos[5] == '3' &&
      ToASCIILower(pos[6]) == 'd' && pos[7] == '(';

  if (is_scale3d) {
    pos += 8;
    CSSFunctionValue* transform_value =
        CSSFunctionValue::Create(CSSValueScale3d);
    if (!ParseTransformNumberArguments(pos, end, 3, transform_value))
      return nullptr;
    return transform_value;
  }

  return nullptr;
}

template <typename CharType>
static bool TransformCanLikelyUseFastPath(const CharType* chars,
                                          unsigned length) {
  // Very fast scan that attempts to reject most transforms that couldn't
  // take the fast path. This avoids doing the malloc and string->double
  // conversions in parseSimpleTransformValue only to discard them when we
  // run into a transform component we don't understand.
  unsigned i = 0;
  while (i < length) {
    if (IsCSSSpace(chars[i])) {
      ++i;
      continue;
    }
    if (length - i < kShortestValidTransformStringLength)
      return false;
    switch (ToASCIILower(chars[i])) {
      case 't':
        // translate, translateX, translateY, translateZ, translate3d.
        if (ToASCIILower(chars[i + 8]) != 'e')
          return false;
        i += 9;
        break;
      case 'm':
        // matrix3d.
        if (ToASCIILower(chars[i + 7]) != 'd')
          return false;
        i += 8;
        break;
      case 's':
        // scale3d.
        if (ToASCIILower(chars[i + 6]) != 'd')
          return false;
        i += 7;
        break;
      default:
        // All other things, ex. rotate.
        return false;
    }
    wtf_size_t arguments_end = WTF::Find(chars, length, ')', i);
    if (arguments_end == kNotFound)
      return false;
    // Advance to the end of the arguments.
    i = arguments_end + 1;
  }
  return i == length;
}

template <typename CharType>
static CSSValueList* ParseSimpleTransformList(const CharType* chars,
                                              unsigned length) {
  if (!TransformCanLikelyUseFastPath(chars, length))
    return nullptr;
  const CharType*& pos = chars;
  const CharType* end = chars + length;
  CSSValueList* transform_list = nullptr;
  while (pos < end) {
    while (pos < end && IsCSSSpace(*pos))
      ++pos;
    if (pos >= end)
      break;
    CSSFunctionValue* transform_value = ParseSimpleTransformValue(pos, end);
    if (!transform_value)
      return nullptr;
    if (!transform_list)
      transform_list = CSSValueList::CreateSpaceSeparated();
    transform_list->Append(*transform_value);
  }
  return transform_list;
}

static CSSValue* ParseSimpleTransform(CSSPropertyID property_id,
                                      const String& string) {
  DCHECK(!string.IsEmpty());

  if (property_id != CSSPropertyID::kTransform)
    return nullptr;
  if (string.Is8Bit())
    return ParseSimpleTransformList(string.Characters8(), string.length());
  return ParseSimpleTransformList(string.Characters16(), string.length());
}

CSSValue* CSSParserFastPaths::MaybeParseValue(CSSPropertyID property_id,
                                              const String& string,
                                              CSSParserMode parser_mode) {
  if (CSSValue* length =
          ParseSimpleLengthValue(property_id, string, parser_mode))
    return length;
  if (IsColorPropertyID(property_id))
    return ParseColor(string, parser_mode);
  if (CSSValue* keyword = ParseKeywordValue(property_id, string, parser_mode))
    return keyword;
  if (CSSValue* transform = ParseSimpleTransform(property_id, string))
    return transform;
  return nullptr;
}

}  // namespace blink

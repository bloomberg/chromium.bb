// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/parser/CSSPropertyParserHelpers.h"

#include "core/StylePropertyShorthand.h"
#include "core/css/CSSCalculationValue.h"
#include "core/css/CSSColorValue.h"
#include "core/css/CSSCrossfadeValue.h"
#include "core/css/CSSGradientValue.h"
#include "core/css/CSSImageSetValue.h"
#include "core/css/CSSImageValue.h"
#include "core/css/CSSInitialValue.h"
#include "core/css/CSSPaintValue.h"
#include "core/css/CSSShadowValue.h"
#include "core/css/CSSStringValue.h"
#include "core/css/CSSURIValue.h"
#include "core/css/CSSValuePair.h"
#include "core/css/CSSVariableData.h"
#include "core/css/StyleColor.h"
#include "core/css/parser/CSSParserContext.h"
#include "core/css/parser/CSSParserFastPaths.h"
#include "core/css/parser/CSSParserLocalContext.h"
#include "core/css/properties/CSSPropertyAPI.h"
#include "core/css/properties/CSSPropertyBoxShadowUtils.h"
#include "core/css/properties/CSSPropertyTransformUtils.h"
#include "core/frame/UseCounter.h"
#include "platform/runtime_enabled_features.h"

namespace blink {

using namespace cssvalue;

namespace CSSPropertyParserHelpers {

namespace {

// Add CSSVariableData to variableData vector.
bool AddCSSPaintArgument(
    const Vector<CSSParserToken>& tokens,
    Vector<scoped_refptr<CSSVariableData>>* const variable_data) {
  CSSParserTokenRange token_range(tokens);
  if (!token_range.AtEnd()) {
    scoped_refptr<CSSVariableData> unparsed_css_variable_data =
        CSSVariableData::Create(token_range, false, false);
    if (unparsed_css_variable_data.get()) {
      variable_data->push_back(std::move(unparsed_css_variable_data));
      return true;
    }
  }
  return false;
}

// Consume input arguments, if encounter function, will return the function
// block as a Vector of CSSParserToken, otherwise, will just return a Vector of
// a single CSSParserToken.
Vector<CSSParserToken> ConsumeFunctionArgsOrNot(CSSParserTokenRange& args) {
  Vector<CSSParserToken> argument_tokens;
  if (args.Peek().GetBlockType() == CSSParserToken::kBlockStart) {
    // Function block.
    // Push the function name and initial right parenthesis.
    // Since we don't have any upfront knowledge about the input argument types
    // here, we should just leave the token as it is and resolve it later in
    // the variable parsing phase.
    argument_tokens.push_back(args.Peek());
    CSSParserTokenRange contents = args.ConsumeBlock();
    while (!contents.AtEnd()) {
      argument_tokens.push_back(contents.Consume());
    }
    argument_tokens.push_back(
        CSSParserToken(kRightParenthesisToken, CSSParserToken::kBlockEnd));

  } else {
    argument_tokens.push_back(args.ConsumeIncludingWhitespace());
  }
  return argument_tokens;
}

CSSFunctionValue* ConsumeFilterFunction(CSSParserTokenRange& range,
                                        const CSSParserContext& context) {
  CSSValueID filter_type = range.Peek().FunctionId();
  if (filter_type < CSSValueInvert || filter_type > CSSValueDropShadow)
    return nullptr;
  CSSParserTokenRange args = CSSPropertyParserHelpers::ConsumeFunction(range);
  CSSFunctionValue* filter_value = CSSFunctionValue::Create(filter_type);
  CSSValue* parsed_value = nullptr;

  if (filter_type == CSSValueDropShadow) {
    parsed_value = CSSPropertyBoxShadowUtils::ParseSingleShadow(
        args, context.Mode(), AllowInsetAndSpread::kForbid);
  } else {
    if (args.AtEnd()) {
      context.Count(WebFeature::kCSSFilterFunctionNoArguments);
      return filter_value;
    }
    if (filter_type == CSSValueBrightness) {
      // FIXME (crbug.com/397061): Support calc expressions like calc(10% + 0.5)
      parsed_value =
          CSSPropertyParserHelpers::ConsumePercent(args, kValueRangeAll);
      if (!parsed_value) {
        parsed_value =
            CSSPropertyParserHelpers::ConsumeNumber(args, kValueRangeAll);
      }
      if (parsed_value &&
          ToCSSPrimitiveValue(parsed_value)->GetDoubleValue() < 0) {
        // crbug.com/776208: Negative values are not allowed by spec.
        context.Count(WebFeature::kCSSFilterFunctionNegativeBrightness);
      }
    } else if (filter_type == CSSValueHueRotate) {
      parsed_value = CSSPropertyParserHelpers::ConsumeAngle(
          args, &context, WebFeature::kUnitlessZeroAngleFilter);
    } else if (filter_type == CSSValueBlur) {
      parsed_value = CSSPropertyParserHelpers::ConsumeLength(
          args, kHTMLStandardMode, kValueRangeNonNegative);
    } else {
      // FIXME (crbug.com/397061): Support calc expressions like calc(10% + 0.5)
      parsed_value = CSSPropertyParserHelpers::ConsumePercent(
          args, kValueRangeNonNegative);
      if (!parsed_value) {
        parsed_value = CSSPropertyParserHelpers::ConsumeNumber(
            args, kValueRangeNonNegative);
      }
      if (parsed_value && filter_type != CSSValueSaturate &&
          filter_type != CSSValueContrast) {
        bool is_percentage = ToCSSPrimitiveValue(parsed_value)->IsPercentage();
        double max_allowed = is_percentage ? 100.0 : 1.0;
        if (ToCSSPrimitiveValue(parsed_value)->GetDoubleValue() > max_allowed) {
          parsed_value = CSSPrimitiveValue::Create(
              max_allowed, is_percentage
                               ? CSSPrimitiveValue::UnitType::kPercentage
                               : CSSPrimitiveValue::UnitType::kNumber);
        }
      }
    }
  }
  if (!parsed_value || !args.AtEnd())
    return nullptr;
  filter_value->Append(*parsed_value);
  return filter_value;
}

}  // namespace

void Complete4Sides(CSSValue* side[4]) {
  if (side[3])
    return;
  if (!side[2]) {
    if (!side[1])
      side[1] = side[0];
    side[2] = side[0];
  }
  side[3] = side[1];
}

bool ConsumeCommaIncludingWhitespace(CSSParserTokenRange& range) {
  CSSParserToken value = range.Peek();
  if (value.GetType() != kCommaToken)
    return false;
  range.ConsumeIncludingWhitespace();
  return true;
}

bool ConsumeSlashIncludingWhitespace(CSSParserTokenRange& range) {
  CSSParserToken value = range.Peek();
  if (value.GetType() != kDelimiterToken || value.Delimiter() != '/')
    return false;
  range.ConsumeIncludingWhitespace();
  return true;
}

CSSParserTokenRange ConsumeFunction(CSSParserTokenRange& range) {
  DCHECK_EQ(range.Peek().GetType(), kFunctionToken);
  CSSParserTokenRange contents = range.ConsumeBlock();
  range.ConsumeWhitespace();
  contents.ConsumeWhitespace();
  return contents;
}

// TODO(rwlbuis): consider pulling in the parsing logic from
// CSSCalculationValue.cpp.
class CalcParser {
  STACK_ALLOCATED();

 public:
  explicit CalcParser(CSSParserTokenRange& range,
                      ValueRange value_range = kValueRangeAll)
      : source_range_(range), range_(range) {
    const CSSParserToken& token = range.Peek();
    if (token.FunctionId() == CSSValueCalc ||
        token.FunctionId() == CSSValueWebkitCalc)
      calc_value_ = CSSCalcValue::Create(ConsumeFunction(range_), value_range);
  }

  const CSSCalcValue* Value() const { return calc_value_.Get(); }
  CSSPrimitiveValue* ConsumeValue() {
    if (!calc_value_)
      return nullptr;
    source_range_ = range_;
    return CSSPrimitiveValue::Create(calc_value_.Release());
  }
  CSSPrimitiveValue* ConsumeNumber() {
    if (!calc_value_)
      return nullptr;
    source_range_ = range_;
    CSSPrimitiveValue::UnitType unit_type =
        calc_value_->IsInt() ? CSSPrimitiveValue::UnitType::kInteger
                             : CSSPrimitiveValue::UnitType::kNumber;
    return CSSPrimitiveValue::Create(calc_value_->DoubleValue(), unit_type);
  }

  bool ConsumeNumberRaw(double& result) {
    if (!calc_value_ || calc_value_->Category() != kCalcNumber)
      return false;
    source_range_ = range_;
    result = calc_value_->DoubleValue();
    return true;
  }

 private:
  CSSParserTokenRange& source_range_;
  CSSParserTokenRange range_;
  Member<CSSCalcValue> calc_value_;
};

CSSPrimitiveValue* ConsumeInteger(CSSParserTokenRange& range,
                                  double minimum_value) {
  const CSSParserToken& token = range.Peek();
  if (token.GetType() == kNumberToken) {
    if (token.GetNumericValueType() == kNumberValueType ||
        token.NumericValue() < minimum_value)
      return nullptr;
    return CSSPrimitiveValue::Create(
        range.ConsumeIncludingWhitespace().NumericValue(),
        CSSPrimitiveValue::UnitType::kInteger);
  }
  CalcParser calc_parser(range);
  if (const CSSCalcValue* calculation = calc_parser.Value()) {
    if (calculation->Category() != kCalcNumber || !calculation->IsInt())
      return nullptr;
    double value = calculation->DoubleValue();
    if (value < minimum_value)
      return nullptr;
    return calc_parser.ConsumeNumber();
  }
  return nullptr;
}

CSSPrimitiveValue* ConsumePositiveInteger(CSSParserTokenRange& range) {
  return ConsumeInteger(range, 1);
}

bool ConsumeNumberRaw(CSSParserTokenRange& range, double& result) {
  if (range.Peek().GetType() == kNumberToken) {
    result = range.ConsumeIncludingWhitespace().NumericValue();
    return true;
  }
  CalcParser calc_parser(range, kValueRangeAll);
  return calc_parser.ConsumeNumberRaw(result);
}

// TODO(timloh): Work out if this can just call consumeNumberRaw
CSSPrimitiveValue* ConsumeNumber(CSSParserTokenRange& range,
                                 ValueRange value_range) {
  const CSSParserToken& token = range.Peek();
  if (token.GetType() == kNumberToken) {
    if (value_range == kValueRangeNonNegative && token.NumericValue() < 0)
      return nullptr;
    return CSSPrimitiveValue::Create(
        range.ConsumeIncludingWhitespace().NumericValue(), token.GetUnitType());
  }
  CalcParser calc_parser(range, kValueRangeAll);
  if (const CSSCalcValue* calculation = calc_parser.Value()) {
    // TODO(rwlbuis) Calcs should not be subject to parse time range checks.
    // spec: https://drafts.csswg.org/css-values-3/#calc-range
    if (calculation->Category() != kCalcNumber ||
        (value_range == kValueRangeNonNegative && calculation->IsNegative()))
      return nullptr;
    return calc_parser.ConsumeNumber();
  }
  return nullptr;
}

inline bool ShouldAcceptUnitlessLength(double value,
                                       CSSParserMode css_parser_mode,
                                       UnitlessQuirk unitless) {
  return value == 0 || css_parser_mode == kSVGAttributeMode ||
         (css_parser_mode == kHTMLQuirksMode &&
          unitless == UnitlessQuirk::kAllow);
}

CSSPrimitiveValue* ConsumeLength(CSSParserTokenRange& range,
                                 CSSParserMode css_parser_mode,
                                 ValueRange value_range,
                                 UnitlessQuirk unitless) {
  const CSSParserToken& token = range.Peek();
  if (token.GetType() == kDimensionToken) {
    switch (token.GetUnitType()) {
      case CSSPrimitiveValue::UnitType::kQuirkyEms:
        if (css_parser_mode != kUASheetMode)
          return nullptr;
      /* fallthrough intentional */
      case CSSPrimitiveValue::UnitType::kEms:
      case CSSPrimitiveValue::UnitType::kRems:
      case CSSPrimitiveValue::UnitType::kChs:
      case CSSPrimitiveValue::UnitType::kExs:
      case CSSPrimitiveValue::UnitType::kPixels:
      case CSSPrimitiveValue::UnitType::kCentimeters:
      case CSSPrimitiveValue::UnitType::kMillimeters:
      case CSSPrimitiveValue::UnitType::kQuarterMillimeters:
      case CSSPrimitiveValue::UnitType::kInches:
      case CSSPrimitiveValue::UnitType::kPoints:
      case CSSPrimitiveValue::UnitType::kPicas:
      case CSSPrimitiveValue::UnitType::kUserUnits:
      case CSSPrimitiveValue::UnitType::kViewportWidth:
      case CSSPrimitiveValue::UnitType::kViewportHeight:
      case CSSPrimitiveValue::UnitType::kViewportMin:
      case CSSPrimitiveValue::UnitType::kViewportMax:
        break;
      default:
        return nullptr;
    }
    if (value_range == kValueRangeNonNegative && token.NumericValue() < 0)
      return nullptr;
    return CSSPrimitiveValue::Create(
        range.ConsumeIncludingWhitespace().NumericValue(), token.GetUnitType());
  }
  if (token.GetType() == kNumberToken) {
    if (!ShouldAcceptUnitlessLength(token.NumericValue(), css_parser_mode,
                                    unitless) ||
        (value_range == kValueRangeNonNegative && token.NumericValue() < 0))
      return nullptr;
    CSSPrimitiveValue::UnitType unit_type =
        CSSPrimitiveValue::UnitType::kPixels;
    if (css_parser_mode == kSVGAttributeMode)
      unit_type = CSSPrimitiveValue::UnitType::kUserUnits;
    return CSSPrimitiveValue::Create(
        range.ConsumeIncludingWhitespace().NumericValue(), unit_type);
  }
  if (css_parser_mode == kSVGAttributeMode)
    return nullptr;
  CalcParser calc_parser(range, value_range);
  if (calc_parser.Value() && calc_parser.Value()->Category() == kCalcLength)
    return calc_parser.ConsumeValue();
  return nullptr;
}

CSSPrimitiveValue* ConsumePercent(CSSParserTokenRange& range,
                                  ValueRange value_range) {
  const CSSParserToken& token = range.Peek();
  if (token.GetType() == kPercentageToken) {
    if (value_range == kValueRangeNonNegative && token.NumericValue() < 0)
      return nullptr;
    return CSSPrimitiveValue::Create(
        range.ConsumeIncludingWhitespace().NumericValue(),
        CSSPrimitiveValue::UnitType::kPercentage);
  }
  CalcParser calc_parser(range, value_range);
  if (const CSSCalcValue* calculation = calc_parser.Value()) {
    if (calculation->Category() == kCalcPercent)
      return calc_parser.ConsumeValue();
  }
  return nullptr;
}

bool CanConsumeCalcValue(CalculationCategory category,
                         CSSParserMode css_parser_mode) {
  if (category == kCalcLength || category == kCalcPercent ||
      category == kCalcPercentLength)
    return true;

  if (css_parser_mode != kSVGAttributeMode)
    return false;

  if (category == kCalcNumber || category == kCalcPercentNumber ||
      category == kCalcLengthNumber || category == kCalcPercentLengthNumber)
    return true;

  return false;
}

CSSPrimitiveValue* ConsumeLengthOrPercent(CSSParserTokenRange& range,
                                          CSSParserMode css_parser_mode,
                                          ValueRange value_range,
                                          UnitlessQuirk unitless) {
  const CSSParserToken& token = range.Peek();
  if (token.GetType() == kDimensionToken || token.GetType() == kNumberToken)
    return ConsumeLength(range, css_parser_mode, value_range, unitless);
  if (token.GetType() == kPercentageToken)
    return ConsumePercent(range, value_range);
  CalcParser calc_parser(range, value_range);
  if (const CSSCalcValue* calculation = calc_parser.Value()) {
    if (CanConsumeCalcValue(calculation->Category(), css_parser_mode))
      return calc_parser.ConsumeValue();
  }
  return nullptr;
}

CSSPrimitiveValue* ConsumeGradientLengthOrPercent(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    ValueRange value_range,
    UnitlessQuirk unitless) {
  return ConsumeLengthOrPercent(range, context.Mode(), value_range, unitless);
}

CSSPrimitiveValue* ConsumeAngle(CSSParserTokenRange& range,
                                const CSSParserContext* context,
                                WTF::Optional<WebFeature> unitlessZeroFeature) {
  // Ensure that we have a context for counting the unitlessZeroFeature if it is
  // requested.
  DCHECK(context || !unitlessZeroFeature);
  const CSSParserToken& token = range.Peek();
  if (token.GetType() == kDimensionToken) {
    switch (token.GetUnitType()) {
      case CSSPrimitiveValue::UnitType::kDegrees:
      case CSSPrimitiveValue::UnitType::kRadians:
      case CSSPrimitiveValue::UnitType::kGradians:
      case CSSPrimitiveValue::UnitType::kTurns:
        return CSSPrimitiveValue::Create(
            range.ConsumeIncludingWhitespace().NumericValue(),
            token.GetUnitType());
      default:
        return nullptr;
    }
  }
  if (token.GetType() == kNumberToken && token.NumericValue() == 0 &&
      unitlessZeroFeature) {
    range.ConsumeIncludingWhitespace();
    context->Count(*unitlessZeroFeature);
    return CSSPrimitiveValue::Create(0, CSSPrimitiveValue::UnitType::kDegrees);
  }
  CalcParser calc_parser(range, kValueRangeAll);
  if (const CSSCalcValue* calculation = calc_parser.Value()) {
    if (calculation->Category() == kCalcAngle)
      return calc_parser.ConsumeValue();
  }
  return nullptr;
}

CSSPrimitiveValue* ConsumeTime(CSSParserTokenRange& range,
                               ValueRange value_range) {
  const CSSParserToken& token = range.Peek();
  if (token.GetType() == kDimensionToken) {
    if (value_range == kValueRangeNonNegative && token.NumericValue() < 0)
      return nullptr;
    CSSPrimitiveValue::UnitType unit = token.GetUnitType();
    if (unit == CSSPrimitiveValue::UnitType::kMilliseconds ||
        unit == CSSPrimitiveValue::UnitType::kSeconds)
      return CSSPrimitiveValue::Create(
          range.ConsumeIncludingWhitespace().NumericValue(),
          token.GetUnitType());
    return nullptr;
  }
  CalcParser calc_parser(range, value_range);
  if (const CSSCalcValue* calculation = calc_parser.Value()) {
    if (calculation->Category() == kCalcTime)
      return calc_parser.ConsumeValue();
  }
  return nullptr;
}

CSSPrimitiveValue* ConsumeResolution(CSSParserTokenRange& range) {
  const CSSParserToken& token = range.Peek();
  // Unlike the other types, calc() does not work with <resolution>.
  if (token.GetType() != kDimensionToken)
    return nullptr;
  CSSPrimitiveValue::UnitType unit = token.GetUnitType();
  if (unit == CSSPrimitiveValue::UnitType::kDotsPerPixel ||
      unit == CSSPrimitiveValue::UnitType::kDotsPerInch ||
      unit == CSSPrimitiveValue::UnitType::kDotsPerCentimeter) {
    return CSSPrimitiveValue::Create(
        range.ConsumeIncludingWhitespace().NumericValue(), unit);
  }
  return nullptr;
}

CSSIdentifierValue* ConsumeIdent(CSSParserTokenRange& range) {
  if (range.Peek().GetType() != kIdentToken)
    return nullptr;
  return CSSIdentifierValue::Create(range.ConsumeIncludingWhitespace().Id());
}

CSSIdentifierValue* ConsumeIdentRange(CSSParserTokenRange& range,
                                      CSSValueID lower,
                                      CSSValueID upper) {
  if (range.Peek().Id() < lower || range.Peek().Id() > upper)
    return nullptr;
  return ConsumeIdent(range);
}

CSSCustomIdentValue* ConsumeCustomIdentWithToken(const CSSParserToken& token) {
  if (token.GetType() != kIdentToken || IsCSSWideKeyword(token.Value()))
    return nullptr;
  return CSSCustomIdentValue::Create(token.Value().ToAtomicString());
}

CSSCustomIdentValue* ConsumeCustomIdent(CSSParserTokenRange& range) {
  if (range.Peek().GetType() != kIdentToken ||
      IsCSSWideKeyword(range.Peek().Value()))
    return nullptr;
  return ConsumeCustomIdentWithToken(range.ConsumeIncludingWhitespace());
}

CSSStringValue* ConsumeString(CSSParserTokenRange& range) {
  if (range.Peek().GetType() != kStringToken)
    return nullptr;
  return CSSStringValue::Create(
      range.ConsumeIncludingWhitespace().Value().ToString());
}

StringView ConsumeUrlAsStringView(CSSParserTokenRange& range) {
  const CSSParserToken& token = range.Peek();
  if (token.GetType() == kUrlToken) {
    range.ConsumeIncludingWhitespace();
    return token.Value();
  }
  if (token.FunctionId() == CSSValueUrl) {
    CSSParserTokenRange url_range = range;
    CSSParserTokenRange url_args = url_range.ConsumeBlock();
    const CSSParserToken& next = url_args.ConsumeIncludingWhitespace();
    if (next.GetType() == kBadStringToken || !url_args.AtEnd())
      return StringView();
    DCHECK_EQ(next.GetType(), kStringToken);
    range = url_range;
    range.ConsumeWhitespace();
    return next.Value();
  }

  return StringView();
}

CSSURIValue* ConsumeUrl(CSSParserTokenRange& range,
                        const CSSParserContext* context) {
  StringView url = ConsumeUrlAsStringView(range);
  if (url.IsNull())
    return nullptr;
  String url_string = url.ToString();
  return CSSURIValue::Create(url_string, context->CompleteURL(url_string));
}

static int ClampRGBComponent(const CSSPrimitiveValue& value) {
  double result = value.GetDoubleValue();
  // TODO(timloh): Multiply by 2.55 and round instead of floor.
  if (value.IsPercentage())
    result *= 2.56;
  return clampTo<int>(result, 0, 255);
}

static bool ParseRGBParameters(CSSParserTokenRange& range,
                               RGBA32& result,
                               bool parse_alpha) {
  DCHECK(range.Peek().FunctionId() == CSSValueRgb ||
         range.Peek().FunctionId() == CSSValueRgba);
  CSSParserTokenRange args = ConsumeFunction(range);
  CSSPrimitiveValue* color_parameter = ConsumeInteger(args);
  if (!color_parameter)
    color_parameter = ConsumePercent(args, kValueRangeAll);
  if (!color_parameter)
    return false;
  const bool is_percent = color_parameter->IsPercentage();
  int color_array[3];
  color_array[0] = ClampRGBComponent(*color_parameter);
  for (int i = 1; i < 3; i++) {
    if (!ConsumeCommaIncludingWhitespace(args))
      return false;
    color_parameter = is_percent ? ConsumePercent(args, kValueRangeAll)
                                 : ConsumeInteger(args);
    if (!color_parameter)
      return false;
    color_array[i] = ClampRGBComponent(*color_parameter);
  }
  if (parse_alpha) {
    if (!ConsumeCommaIncludingWhitespace(args))
      return false;
    double alpha;
    if (!ConsumeNumberRaw(args, alpha))
      return false;
    // W3 standard stipulates a 2.55 alpha value multiplication factor.
    int alpha_component =
        static_cast<int>(lroundf(clampTo<double>(alpha, 0.0, 1.0) * 255.0f));
    result = MakeRGBA(color_array[0], color_array[1], color_array[2],
                      alpha_component);
  } else {
    result = MakeRGB(color_array[0], color_array[1], color_array[2]);
  }
  return args.AtEnd();
}

static bool ParseHSLParameters(CSSParserTokenRange& range,
                               RGBA32& result,
                               bool parse_alpha) {
  DCHECK(range.Peek().FunctionId() == CSSValueHsl ||
         range.Peek().FunctionId() == CSSValueHsla);
  CSSParserTokenRange args = ConsumeFunction(range);
  CSSPrimitiveValue* hsl_value = ConsumeNumber(args, kValueRangeAll);
  if (!hsl_value)
    return false;
  double color_array[3];
  color_array[0] = (((hsl_value->GetIntValue() % 360) + 360) % 360) / 360.0;
  for (int i = 1; i < 3; i++) {
    if (!ConsumeCommaIncludingWhitespace(args))
      return false;
    hsl_value = ConsumePercent(args, kValueRangeAll);
    if (!hsl_value)
      return false;
    double double_value = hsl_value->GetDoubleValue();
    color_array[i] = clampTo<double>(double_value, 0.0, 100.0) /
                     100.0;  // Needs to be value between 0 and 1.0.
  }
  double alpha = 1.0;
  if (parse_alpha) {
    if (!ConsumeCommaIncludingWhitespace(args))
      return false;
    if (!ConsumeNumberRaw(args, alpha))
      return false;
    alpha = clampTo<double>(alpha, 0.0, 1.0);
  }
  result =
      MakeRGBAFromHSLA(color_array[0], color_array[1], color_array[2], alpha);
  return args.AtEnd();
}

static bool ParseHexColor(CSSParserTokenRange& range,
                          RGBA32& result,
                          bool accept_quirky_colors) {
  const CSSParserToken& token = range.Peek();
  if (token.GetType() == kHashToken) {
    if (!Color::ParseHexColor(token.Value(), result))
      return false;
  } else if (accept_quirky_colors) {
    String color;
    if (token.GetType() == kNumberToken || token.GetType() == kDimensionToken) {
      if (token.GetNumericValueType() != kIntegerValueType ||
          token.NumericValue() < 0. || token.NumericValue() >= 1000000.)
        return false;
      if (token.GetType() == kNumberToken)  // e.g. 112233
        color = String::Format("%d", static_cast<int>(token.NumericValue()));
      else  // e.g. 0001FF
        color = String::Number(static_cast<int>(token.NumericValue())) +
                token.Value().ToString();
      while (color.length() < 6)
        color = "0" + color;
    } else if (token.GetType() == kIdentToken) {  // e.g. FF0000
      color = token.Value().ToString();
    }
    unsigned length = color.length();
    if (length != 3 && length != 6)
      return false;
    if (!Color::ParseHexColor(color, result))
      return false;
  } else {
    return false;
  }
  range.ConsumeIncludingWhitespace();
  return true;
}

static bool ParseColorFunction(CSSParserTokenRange& range, RGBA32& result) {
  CSSValueID function_id = range.Peek().FunctionId();
  if (function_id < CSSValueRgb || function_id > CSSValueHsla)
    return false;
  CSSParserTokenRange color_range = range;
  if ((function_id <= CSSValueRgba &&
       !ParseRGBParameters(color_range, result, function_id == CSSValueRgba)) ||
      (function_id >= CSSValueHsl &&
       !ParseHSLParameters(color_range, result, function_id == CSSValueHsla)))
    return false;
  range = color_range;
  return true;
}

CSSValue* ConsumeColor(CSSParserTokenRange& range,
                       CSSParserMode css_parser_mode,
                       bool accept_quirky_colors) {
  CSSValueID id = range.Peek().Id();
  if (StyleColor::IsColorKeyword(id)) {
    if (!isValueAllowedInMode(id, css_parser_mode))
      return nullptr;
    return ConsumeIdent(range);
  }
  RGBA32 color = Color::kTransparent;
  if (!ParseHexColor(range, color, accept_quirky_colors) &&
      !ParseColorFunction(range, color))
    return nullptr;
  return CSSColorValue::Create(color);
}

CSSValue* ConsumeLineWidth(CSSParserTokenRange& range,
                           CSSParserMode css_parser_mode,
                           UnitlessQuirk unitless) {
  CSSValueID id = range.Peek().Id();
  if (id == CSSValueThin || id == CSSValueMedium || id == CSSValueThick)
    return ConsumeIdent(range);
  return ConsumeLength(range, css_parser_mode, kValueRangeNonNegative,
                       unitless);
}

static CSSValue* ConsumePositionComponent(CSSParserTokenRange& range,
                                          CSSParserMode css_parser_mode,
                                          UnitlessQuirk unitless,
                                          bool& horizontal_edge,
                                          bool& vertical_edge) {
  if (range.Peek().GetType() != kIdentToken)
    return ConsumeLengthOrPercent(range, css_parser_mode, kValueRangeAll,
                                  unitless);

  CSSValueID id = range.Peek().Id();
  if (id == CSSValueLeft || id == CSSValueRight) {
    if (horizontal_edge)
      return nullptr;
    horizontal_edge = true;
  } else if (id == CSSValueTop || id == CSSValueBottom) {
    if (vertical_edge)
      return nullptr;
    vertical_edge = true;
  } else if (id != CSSValueCenter) {
    return nullptr;
  }
  return ConsumeIdent(range);
}

static bool IsHorizontalPositionKeywordOnly(const CSSValue& value) {
  if (!value.IsIdentifierValue())
    return false;
  CSSValueID value_id = ToCSSIdentifierValue(value).GetValueID();
  return value_id == CSSValueLeft || value_id == CSSValueRight;
}

static bool IsVerticalPositionKeywordOnly(const CSSValue& value) {
  if (!value.IsIdentifierValue())
    return false;
  CSSValueID value_id = ToCSSIdentifierValue(value).GetValueID();
  return value_id == CSSValueTop || value_id == CSSValueBottom;
}

static void PositionFromOneValue(CSSValue* value,
                                 CSSValue*& result_x,
                                 CSSValue*& result_y) {
  bool value_applies_to_y_axis_only = IsVerticalPositionKeywordOnly(*value);
  result_x = value;
  result_y = CSSIdentifierValue::Create(CSSValueCenter);
  if (value_applies_to_y_axis_only)
    std::swap(result_x, result_y);
}

static void PositionFromTwoValues(CSSValue* value1,
                                  CSSValue* value2,
                                  CSSValue*& result_x,
                                  CSSValue*& result_y) {
  bool must_order_as_xy = IsHorizontalPositionKeywordOnly(*value1) ||
                          IsVerticalPositionKeywordOnly(*value2) ||
                          !value1->IsIdentifierValue() ||
                          !value2->IsIdentifierValue();
  bool must_order_as_yx = IsVerticalPositionKeywordOnly(*value1) ||
                          IsHorizontalPositionKeywordOnly(*value2);
  DCHECK(!must_order_as_xy || !must_order_as_yx);
  result_x = value1;
  result_y = value2;
  if (must_order_as_yx)
    std::swap(result_x, result_y);
}

static void PositionFromThreeOrFourValues(CSSValue** values,
                                          CSSValue*& result_x,
                                          CSSValue*& result_y) {
  CSSIdentifierValue* center = nullptr;
  for (int i = 0; values[i]; i++) {
    CSSIdentifierValue* current_value = ToCSSIdentifierValue(values[i]);
    CSSValueID id = current_value->GetValueID();

    if (id == CSSValueCenter) {
      DCHECK(!center);
      center = current_value;
      continue;
    }

    CSSValue* result = nullptr;
    if (values[i + 1] && !values[i + 1]->IsIdentifierValue()) {
      result = CSSValuePair::Create(current_value, values[++i],
                                    CSSValuePair::kKeepIdenticalValues);
    } else {
      result = current_value;
    }

    if (id == CSSValueLeft || id == CSSValueRight) {
      DCHECK(!result_x);
      result_x = result;
    } else {
      DCHECK(id == CSSValueTop || id == CSSValueBottom);
      DCHECK(!result_y);
      result_y = result;
    }
  }

  if (center) {
    DCHECK(!!result_x != !!result_y);
    if (!result_x)
      result_x = center;
    else
      result_y = center;
  }

  DCHECK(result_x && result_y);
}

bool ConsumePosition(CSSParserTokenRange& range,
                     const CSSParserContext& context,
                     UnitlessQuirk unitless,
                     WTF::Optional<WebFeature> threeValuePosition,
                     CSSValue*& result_x,
                     CSSValue*& result_y) {
  bool horizontal_edge = false;
  bool vertical_edge = false;
  CSSValue* value1 = ConsumePositionComponent(range, context.Mode(), unitless,
                                              horizontal_edge, vertical_edge);
  if (!value1)
    return false;
  if (!value1->IsIdentifierValue())
    horizontal_edge = true;

  CSSParserTokenRange range_after_first_consume = range;
  CSSValue* value2 = ConsumePositionComponent(range, context.Mode(), unitless,
                                              horizontal_edge, vertical_edge);
  if (!value2) {
    PositionFromOneValue(value1, result_x, result_y);
    return true;
  }

  CSSParserTokenRange range_after_second_consume = range;
  CSSValue* value3 = nullptr;
  if (value1->IsIdentifierValue() &&
      value2->IsIdentifierValue() != (range.Peek().GetType() == kIdentToken) &&
      (value2->IsIdentifierValue()
           ? ToCSSIdentifierValue(value2)->GetValueID()
           : ToCSSIdentifierValue(value1)->GetValueID()) != CSSValueCenter)
    value3 = ConsumePositionComponent(range, context.Mode(), unitless,
                                      horizontal_edge, vertical_edge);
  if (!value3) {
    if (vertical_edge && !value2->IsIdentifierValue()) {
      range = range_after_first_consume;
      PositionFromOneValue(value1, result_x, result_y);
      return true;
    }
    PositionFromTwoValues(value1, value2, result_x, result_y);
    return true;
  }

  CSSValue* value4 = nullptr;
  if (value3->IsIdentifierValue() &&
      ToCSSIdentifierValue(value3)->GetValueID() != CSSValueCenter &&
      range.Peek().GetType() != kIdentToken)
    value4 = ConsumePositionComponent(range, context.Mode(), unitless,
                                      horizontal_edge, vertical_edge);

  if (!value4) {
    if (!threeValuePosition) {
      // [top | bottom] <length-percentage> is not permitted
      if (vertical_edge && !value2->IsIdentifierValue()) {
        range = range_after_first_consume;
        PositionFromOneValue(value1, result_x, result_y);
        return true;
      }
      range = range_after_second_consume;
      PositionFromTwoValues(value1, value2, result_x, result_y);
      return true;
    }
    context.Count(*threeValuePosition);
  }

  CSSValue* values[5];
  values[0] = value1;
  values[1] = value2;
  values[2] = value3;
  values[3] = value4;
  values[4] = nullptr;
  PositionFromThreeOrFourValues(values, result_x, result_y);
  return true;
}

CSSValuePair* ConsumePosition(CSSParserTokenRange& range,
                              const CSSParserContext& context,
                              UnitlessQuirk unitless,
                              WTF::Optional<WebFeature> threeValuePosition) {
  CSSValue* result_x = nullptr;
  CSSValue* result_y = nullptr;
  if (ConsumePosition(range, context, unitless, threeValuePosition, result_x,
                      result_y))
    return CSSValuePair::Create(result_x, result_y,
                                CSSValuePair::kKeepIdenticalValues);
  return nullptr;
}

bool ConsumeOneOrTwoValuedPosition(CSSParserTokenRange& range,
                                   CSSParserMode css_parser_mode,
                                   UnitlessQuirk unitless,
                                   CSSValue*& result_x,
                                   CSSValue*& result_y) {
  bool horizontal_edge = false;
  bool vertical_edge = false;
  CSSValue* value1 = ConsumePositionComponent(range, css_parser_mode, unitless,
                                              horizontal_edge, vertical_edge);
  if (!value1)
    return false;
  if (!value1->IsIdentifierValue())
    horizontal_edge = true;

  CSSValue* value2 = nullptr;
  if (!vertical_edge || range.Peek().GetType() == kIdentToken)
    value2 = ConsumePositionComponent(range, css_parser_mode, unitless,
                                      horizontal_edge, vertical_edge);
  if (!value2) {
    PositionFromOneValue(value1, result_x, result_y);
    return true;
  }
  PositionFromTwoValues(value1, value2, result_x, result_y);
  return true;
}

// This should go away once we drop support for -webkit-gradient
static CSSPrimitiveValue* ConsumeDeprecatedGradientPoint(
    CSSParserTokenRange& args,
    bool horizontal) {
  if (args.Peek().GetType() == kIdentToken) {
    if ((horizontal && ConsumeIdent<CSSValueLeft>(args)) ||
        (!horizontal && ConsumeIdent<CSSValueTop>(args)))
      return CSSPrimitiveValue::Create(
          0., CSSPrimitiveValue::UnitType::kPercentage);
    if ((horizontal && ConsumeIdent<CSSValueRight>(args)) ||
        (!horizontal && ConsumeIdent<CSSValueBottom>(args)))
      return CSSPrimitiveValue::Create(
          100., CSSPrimitiveValue::UnitType::kPercentage);
    if (ConsumeIdent<CSSValueCenter>(args))
      return CSSPrimitiveValue::Create(
          50., CSSPrimitiveValue::UnitType::kPercentage);
    return nullptr;
  }
  CSSPrimitiveValue* result = ConsumePercent(args, kValueRangeAll);
  if (!result)
    result = ConsumeNumber(args, kValueRangeAll);
  return result;
}

// Used to parse colors for -webkit-gradient(...).
static CSSValue* ConsumeDeprecatedGradientStopColor(
    CSSParserTokenRange& args,
    CSSParserMode css_parser_mode) {
  if (args.Peek().Id() == CSSValueCurrentcolor)
    return nullptr;
  return ConsumeColor(args, css_parser_mode);
}

static bool ConsumeDeprecatedGradientColorStop(CSSParserTokenRange& range,
                                               CSSGradientColorStop& stop,
                                               CSSParserMode css_parser_mode) {
  CSSValueID id = range.Peek().FunctionId();
  if (id != CSSValueFrom && id != CSSValueTo && id != CSSValueColorStop)
    return false;

  CSSParserTokenRange args = ConsumeFunction(range);
  double position;
  if (id == CSSValueFrom || id == CSSValueTo) {
    position = (id == CSSValueFrom) ? 0 : 1;
  } else {
    DCHECK(id == CSSValueColorStop);
    const CSSParserToken& arg = args.ConsumeIncludingWhitespace();
    if (arg.GetType() == kPercentageToken)
      position = arg.NumericValue() / 100.0;
    else if (arg.GetType() == kNumberToken)
      position = arg.NumericValue();
    else
      return false;

    if (!ConsumeCommaIncludingWhitespace(args))
      return false;
  }

  stop.offset_ =
      CSSPrimitiveValue::Create(position, CSSPrimitiveValue::UnitType::kNumber);
  stop.color_ = ConsumeDeprecatedGradientStopColor(args, css_parser_mode);
  return stop.color_ && args.AtEnd();
}

static CSSValue* ConsumeDeprecatedGradient(CSSParserTokenRange& args,
                                           CSSParserMode css_parser_mode) {
  CSSValueID id = args.ConsumeIncludingWhitespace().Id();
  if (id != CSSValueRadial && id != CSSValueLinear)
    return nullptr;

  if (!ConsumeCommaIncludingWhitespace(args))
    return nullptr;

  const CSSPrimitiveValue* first_x = ConsumeDeprecatedGradientPoint(args, true);
  if (!first_x)
    return nullptr;
  const CSSPrimitiveValue* first_y =
      ConsumeDeprecatedGradientPoint(args, false);
  if (!first_y)
    return nullptr;
  if (!ConsumeCommaIncludingWhitespace(args))
    return nullptr;

  // For radial gradients only, we now expect a numeric radius.
  const CSSPrimitiveValue* first_radius = nullptr;
  if (id == CSSValueRadial) {
    first_radius = ConsumeNumber(args, kValueRangeAll);
    if (!first_radius || !ConsumeCommaIncludingWhitespace(args))
      return nullptr;
  }

  const CSSPrimitiveValue* second_x =
      ConsumeDeprecatedGradientPoint(args, true);
  if (!second_x)
    return nullptr;
  const CSSPrimitiveValue* second_y =
      ConsumeDeprecatedGradientPoint(args, false);
  if (!second_y)
    return nullptr;

  // For radial gradients only, we now expect the second radius.
  const CSSPrimitiveValue* second_radius = nullptr;
  if (id == CSSValueRadial) {
    if (!ConsumeCommaIncludingWhitespace(args))
      return nullptr;
    second_radius = ConsumeNumber(args, kValueRangeAll);
    if (!second_radius)
      return nullptr;
  }

  CSSGradientValue* result =
      (id == CSSValueRadial)
          ? CSSRadialGradientValue::Create(
                first_x, first_y, first_radius, second_x, second_y,
                second_radius, kNonRepeating, kCSSDeprecatedRadialGradient)
          : CSSLinearGradientValue::Create(first_x, first_y, second_x, second_y,
                                           nullptr, kNonRepeating,
                                           kCSSDeprecatedLinearGradient);
  CSSGradientColorStop stop;
  while (ConsumeCommaIncludingWhitespace(args)) {
    if (!ConsumeDeprecatedGradientColorStop(args, stop, css_parser_mode))
      return nullptr;
    result->AddStop(stop);
  }

  return result;
}

static CSSPrimitiveValue* ConsumeGradientAngleOrPercent(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    ValueRange value_range,
    UnitlessQuirk) {
  const CSSParserToken& token = range.Peek();
  if (token.GetType() == kDimensionToken || token.GetType() == kNumberToken) {
    return ConsumeAngle(range, &context,
                        WebFeature::kUnitlessZeroAngleGradient);
  }
  if (token.GetType() == kPercentageToken)
    return ConsumePercent(range, value_range);
  CalcParser calc_parser(range, value_range);
  if (const CSSCalcValue* calculation = calc_parser.Value()) {
    CalculationCategory category = calculation->Category();
    // TODO(fs): Add and support kCalcPercentAngle?
    if (category == kCalcAngle || category == kCalcPercent)
      return calc_parser.ConsumeValue();
  }
  return nullptr;
}

using PositionFunctor = CSSPrimitiveValue* (*)(CSSParserTokenRange&,
                                               const CSSParserContext&,
                                               ValueRange,
                                               UnitlessQuirk);

static bool ConsumeGradientColorStops(CSSParserTokenRange& range,
                                      const CSSParserContext& context,
                                      CSSGradientValue* gradient,
                                      PositionFunctor consume_position_func) {
  bool supports_color_hints = gradient->GradientType() == kCSSLinearGradient ||
                              gradient->GradientType() == kCSSRadialGradient ||
                              gradient->GradientType() == kCSSConicGradient;

  // The first color stop cannot be a color hint.
  bool previous_stop_was_color_hint = true;
  do {
    CSSGradientColorStop stop;
    stop.color_ = ConsumeColor(range, context.Mode());
    // Two hints in a row are not allowed.
    if (!stop.color_ && (!supports_color_hints || previous_stop_was_color_hint))
      return false;
    previous_stop_was_color_hint = !stop.color_;
    stop.offset_ = consume_position_func(range, context, kValueRangeAll,
                                         UnitlessQuirk::kForbid);
    if (!stop.color_ && !stop.offset_)
      return false;
    gradient->AddStop(stop);

    if (RuntimeEnabledFeatures::MultipleColorStopPositionsEnabled()) {
      if (!stop.color_ || !stop.offset_)
        continue;

      // Optional second position.
      stop.offset_ = consume_position_func(range, context, kValueRangeAll,
                                           UnitlessQuirk::kForbid);
      if (stop.offset_)
        gradient->AddStop(stop);
    }
  } while (ConsumeCommaIncludingWhitespace(range));

  // The last color stop cannot be a color hint.
  if (previous_stop_was_color_hint)
    return false;

  // Must have 2 or more stops to be valid.
  return gradient->StopCount() >= 2;
}

static CSSValue* ConsumeDeprecatedRadialGradient(
    CSSParserTokenRange& args,
    const CSSParserContext& context,
    CSSGradientRepeat repeating) {
  CSSValue* center_x = nullptr;
  CSSValue* center_y = nullptr;
  ConsumeOneOrTwoValuedPosition(args, context.Mode(), UnitlessQuirk::kForbid,
                                center_x, center_y);
  if ((center_x || center_y) && !ConsumeCommaIncludingWhitespace(args))
    return nullptr;

  const CSSIdentifierValue* shape =
      ConsumeIdent<CSSValueCircle, CSSValueEllipse>(args);
  const CSSIdentifierValue* size_keyword =
      ConsumeIdent<CSSValueClosestSide, CSSValueClosestCorner,
                   CSSValueFarthestSide, CSSValueFarthestCorner,
                   CSSValueContain, CSSValueCover>(args);
  if (!shape)
    shape = ConsumeIdent<CSSValueCircle, CSSValueEllipse>(args);

  // Or, two lengths or percentages
  const CSSPrimitiveValue* horizontal_size = nullptr;
  const CSSPrimitiveValue* vertical_size = nullptr;
  if (!shape && !size_keyword) {
    horizontal_size =
        ConsumeLengthOrPercent(args, context.Mode(), kValueRangeAll);
    if (horizontal_size) {
      vertical_size =
          ConsumeLengthOrPercent(args, context.Mode(), kValueRangeAll);
      if (!vertical_size)
        return nullptr;
      ConsumeCommaIncludingWhitespace(args);
    }
  } else {
    ConsumeCommaIncludingWhitespace(args);
  }

  CSSGradientValue* result = CSSRadialGradientValue::Create(
      center_x, center_y, shape, size_keyword, horizontal_size, vertical_size,
      repeating, kCSSPrefixedRadialGradient);
  return ConsumeGradientColorStops(args, context, result,
                                   ConsumeGradientLengthOrPercent)
             ? result
             : nullptr;
}

static CSSValue* ConsumeRadialGradient(CSSParserTokenRange& args,
                                       const CSSParserContext& context,
                                       CSSGradientRepeat repeating) {
  const CSSIdentifierValue* shape = nullptr;
  const CSSIdentifierValue* size_keyword = nullptr;
  const CSSPrimitiveValue* horizontal_size = nullptr;
  const CSSPrimitiveValue* vertical_size = nullptr;

  // First part of grammar, the size/shape clause:
  // [ circle || <length> ] |
  // [ ellipse || [ <length> | <percentage> ]{2} ] |
  // [ [ circle | ellipse] || <size-keyword> ]
  for (int i = 0; i < 3; ++i) {
    if (args.Peek().GetType() == kIdentToken) {
      CSSValueID id = args.Peek().Id();
      if (id == CSSValueCircle || id == CSSValueEllipse) {
        if (shape)
          return nullptr;
        shape = ConsumeIdent(args);
      } else if (id == CSSValueClosestSide || id == CSSValueClosestCorner ||
                 id == CSSValueFarthestSide || id == CSSValueFarthestCorner) {
        if (size_keyword)
          return nullptr;
        size_keyword = ConsumeIdent(args);
      } else {
        break;
      }
    } else {
      CSSPrimitiveValue* center =
          ConsumeLengthOrPercent(args, context.Mode(), kValueRangeAll);
      if (!center)
        break;
      if (horizontal_size)
        return nullptr;
      horizontal_size = center;
      center = ConsumeLengthOrPercent(args, context.Mode(), kValueRangeAll);
      if (center) {
        vertical_size = center;
        ++i;
      }
    }
  }

  // You can specify size as a keyword or a length/percentage, not both.
  if (size_keyword && horizontal_size)
    return nullptr;
  // Circles must have 0 or 1 lengths.
  if (shape && shape->GetValueID() == CSSValueCircle && vertical_size)
    return nullptr;
  // Ellipses must have 0 or 2 length/percentages.
  if (shape && shape->GetValueID() == CSSValueEllipse && horizontal_size &&
      !vertical_size) {
    return nullptr;
  }
  // If there's only one size, it must be a length.
  if (!vertical_size && horizontal_size && horizontal_size->IsPercentage())
    return nullptr;
  if ((horizontal_size &&
       horizontal_size->IsCalculatedPercentageWithLength()) ||
      (vertical_size && vertical_size->IsCalculatedPercentageWithLength())) {
    return nullptr;
  }

  CSSValue* center_x = nullptr;
  CSSValue* center_y = nullptr;
  if (args.Peek().Id() == CSSValueAt) {
    args.ConsumeIncludingWhitespace();
    ConsumePosition(args, context, UnitlessQuirk::kForbid,
                    WebFeature::kThreeValuedPositionGradient, center_x,
                    center_y);
    if (!(center_x && center_y))
      return nullptr;
    // Right now, CSS radial gradients have the same start and end centers.
  }

  if ((shape || size_keyword || horizontal_size || center_x || center_y) &&
      !ConsumeCommaIncludingWhitespace(args)) {
    return nullptr;
  }

  CSSGradientValue* result = CSSRadialGradientValue::Create(
      center_x, center_y, shape, size_keyword, horizontal_size, vertical_size,
      repeating, kCSSRadialGradient);
  return ConsumeGradientColorStops(args, context, result,
                                   ConsumeGradientLengthOrPercent)
             ? result
             : nullptr;
}

static CSSValue* ConsumeLinearGradient(CSSParserTokenRange& args,
                                       const CSSParserContext& context,
                                       CSSGradientRepeat repeating,
                                       CSSGradientType gradient_type) {
  bool expect_comma = true;
  const CSSPrimitiveValue* angle =
      ConsumeAngle(args, &context, WebFeature::kUnitlessZeroAngleGradient);
  const CSSIdentifierValue* end_x = nullptr;
  const CSSIdentifierValue* end_y = nullptr;
  if (!angle) {
    if (gradient_type == kCSSPrefixedLinearGradient ||
        ConsumeIdent<CSSValueTo>(args)) {
      end_x = ConsumeIdent<CSSValueLeft, CSSValueRight>(args);
      end_y = ConsumeIdent<CSSValueBottom, CSSValueTop>(args);
      if (!end_x && !end_y) {
        if (gradient_type == kCSSLinearGradient)
          return nullptr;
        end_y = CSSIdentifierValue::Create(CSSValueTop);
        expect_comma = false;
      } else if (!end_x) {
        end_x = ConsumeIdent<CSSValueLeft, CSSValueRight>(args);
      }
    } else {
      expect_comma = false;
    }
  }

  if (expect_comma && !ConsumeCommaIncludingWhitespace(args))
    return nullptr;

  CSSGradientValue* result = CSSLinearGradientValue::Create(
      end_x, end_y, nullptr, nullptr, angle, repeating, gradient_type);
  return ConsumeGradientColorStops(args, context, result,
                                   ConsumeGradientLengthOrPercent)
             ? result
             : nullptr;
}

static CSSValue* ConsumeConicGradient(CSSParserTokenRange& args,
                                      const CSSParserContext& context,
                                      CSSGradientRepeat repeating) {
  if (!RuntimeEnabledFeatures::ConicGradientEnabled())
    return nullptr;

  const CSSPrimitiveValue* from_angle = nullptr;
  if (ConsumeIdent<CSSValueFrom>(args)) {
    if (!(from_angle = ConsumeAngle(args, &context,
                                    WebFeature::kUnitlessZeroAngleGradient)))
      return nullptr;
  }

  CSSValue* center_x = nullptr;
  CSSValue* center_y = nullptr;
  if (ConsumeIdent<CSSValueAt>(args)) {
    if (!ConsumePosition(args, context, UnitlessQuirk::kForbid,
                         WebFeature::kThreeValuedPositionGradient, center_x,
                         center_y))
      return nullptr;
  }

  // Comma separator required when fromAngle or position is present.
  if ((from_angle || center_x || center_y) &&
      !ConsumeCommaIncludingWhitespace(args)) {
    return nullptr;
  }

  CSSGradientValue* result =
      CSSConicGradientValue::Create(center_x, center_y, from_angle, repeating);
  return ConsumeGradientColorStops(args, context, result,
                                   ConsumeGradientAngleOrPercent)
             ? result
             : nullptr;
}

CSSValue* ConsumeImageOrNone(CSSParserTokenRange& range,
                             const CSSParserContext* context) {
  if (range.Peek().Id() == CSSValueNone)
    return ConsumeIdent(range);
  return ConsumeImage(range, context);
}

static CSSValue* ConsumeCrossFade(CSSParserTokenRange& args,
                                  const CSSParserContext* context) {
  CSSValue* from_image_value = ConsumeImageOrNone(args, context);
  if (!from_image_value || !ConsumeCommaIncludingWhitespace(args))
    return nullptr;
  CSSValue* to_image_value = ConsumeImageOrNone(args, context);
  if (!to_image_value || !ConsumeCommaIncludingWhitespace(args))
    return nullptr;

  CSSPrimitiveValue* percentage = nullptr;
  const CSSParserToken& percentage_arg = args.ConsumeIncludingWhitespace();
  if (percentage_arg.GetType() == kPercentageToken)
    percentage = CSSPrimitiveValue::Create(
        clampTo<double>(percentage_arg.NumericValue() / 100, 0, 1),
        CSSPrimitiveValue::UnitType::kNumber);
  else if (percentage_arg.GetType() == kNumberToken)
    percentage = CSSPrimitiveValue::Create(
        clampTo<double>(percentage_arg.NumericValue(), 0, 1),
        CSSPrimitiveValue::UnitType::kNumber);

  if (!percentage)
    return nullptr;
  return CSSCrossfadeValue::Create(from_image_value, to_image_value,
                                   percentage);
}

static CSSValue* ConsumePaint(CSSParserTokenRange& args,
                              const CSSParserContext* context) {
  DCHECK(RuntimeEnabledFeatures::CSSPaintAPIEnabled());

  const CSSParserToken& name_token = args.ConsumeIncludingWhitespace();
  CSSCustomIdentValue* name = ConsumeCustomIdentWithToken(name_token);
  if (!name)
    return nullptr;

  if (args.AtEnd())
    return CSSPaintValue::Create(name);

  if (!RuntimeEnabledFeatures::CSSPaintAPIArgumentsEnabled()) {
    // Arguments not enabled, but exists. Invalid.
    return nullptr;
  }

  // Begin parse paint arguments.
  if (!ConsumeCommaIncludingWhitespace(args))
    return nullptr;

  // Consume arguments.
  // TODO(renjieliu): We may want to optimize the implementation by resolve
  // variables early if paint function is registered.
  Vector<CSSParserToken> argument_tokens;
  Vector<scoped_refptr<CSSVariableData>> variable_data;
  while (!args.AtEnd()) {
    if (args.Peek().GetType() != kCommaToken) {
      argument_tokens.AppendVector(ConsumeFunctionArgsOrNot(args));
    } else {
      if (!AddCSSPaintArgument(argument_tokens, &variable_data))
        return nullptr;
      argument_tokens.clear();
      if (!ConsumeCommaIncludingWhitespace(args))
        return nullptr;
    }
  }
  if (!AddCSSPaintArgument(argument_tokens, &variable_data))
    return nullptr;

  return CSSPaintValue::Create(name, variable_data);
}

static CSSValue* ConsumeGeneratedImage(CSSParserTokenRange& range,
                                       const CSSParserContext* context) {
  CSSValueID id = range.Peek().FunctionId();
  CSSParserTokenRange range_copy = range;
  CSSParserTokenRange args = ConsumeFunction(range_copy);
  CSSValue* result = nullptr;
  if (id == CSSValueRadialGradient) {
    result = ConsumeRadialGradient(args, *context, kNonRepeating);
  } else if (id == CSSValueRepeatingRadialGradient) {
    result = ConsumeRadialGradient(args, *context, kRepeating);
  } else if (id == CSSValueWebkitLinearGradient) {
    context->Count(WebFeature::kDeprecatedWebKitLinearGradient);
    result = ConsumeLinearGradient(args, *context, kNonRepeating,
                                   kCSSPrefixedLinearGradient);
  } else if (id == CSSValueWebkitRepeatingLinearGradient) {
    context->Count(WebFeature::kDeprecatedWebKitRepeatingLinearGradient);
    result = ConsumeLinearGradient(args, *context, kRepeating,
                                   kCSSPrefixedLinearGradient);
  } else if (id == CSSValueRepeatingLinearGradient) {
    result =
        ConsumeLinearGradient(args, *context, kRepeating, kCSSLinearGradient);
  } else if (id == CSSValueLinearGradient) {
    result = ConsumeLinearGradient(args, *context, kNonRepeating,
                                   kCSSLinearGradient);
  } else if (id == CSSValueWebkitGradient) {
    context->Count(WebFeature::kDeprecatedWebKitGradient);
    result = ConsumeDeprecatedGradient(args, context->Mode());
  } else if (id == CSSValueWebkitRadialGradient) {
    context->Count(WebFeature::kDeprecatedWebKitRadialGradient);
    result = ConsumeDeprecatedRadialGradient(args, *context, kNonRepeating);
  } else if (id == CSSValueWebkitRepeatingRadialGradient) {
    context->Count(WebFeature::kDeprecatedWebKitRepeatingRadialGradient);
    result = ConsumeDeprecatedRadialGradient(args, *context, kRepeating);
  } else if (id == CSSValueConicGradient) {
    result = ConsumeConicGradient(args, *context, kNonRepeating);
  } else if (id == CSSValueRepeatingConicGradient) {
    result = ConsumeConicGradient(args, *context, kRepeating);
  } else if (id == CSSValueWebkitCrossFade) {
    result = ConsumeCrossFade(args, context);
  } else if (id == CSSValuePaint) {
    result = RuntimeEnabledFeatures::CSSPaintAPIEnabled()
                 ? ConsumePaint(args, context)
                 : nullptr;
  }
  if (!result || !args.AtEnd())
    return nullptr;
  range = range_copy;
  return result;
}

static CSSValue* CreateCSSImageValueWithReferrer(
    const AtomicString& raw_value,
    const CSSParserContext* context) {
  CSSValue* image_value = CSSImageValue::Create(
      raw_value, context->CompleteURL(raw_value), context->GetReferrer());
  return image_value;
}

static CSSValue* ConsumeImageSet(CSSParserTokenRange& range,
                                 const CSSParserContext* context) {
  CSSParserTokenRange range_copy = range;
  CSSParserTokenRange args = ConsumeFunction(range_copy);
  CSSImageSetValue* image_set = CSSImageSetValue::Create(context->Mode());
  do {
    AtomicString url_value = ConsumeUrlAsStringView(args).ToAtomicString();
    if (url_value.IsNull())
      return nullptr;

    CSSValue* image = CreateCSSImageValueWithReferrer(url_value, context);
    image_set->Append(*image);

    const CSSParserToken& token = args.ConsumeIncludingWhitespace();
    if (token.GetType() != kDimensionToken)
      return nullptr;
    if (token.Value() != "x")
      return nullptr;
    DCHECK(token.GetUnitType() == CSSPrimitiveValue::UnitType::kUnknown);
    double image_scale_factor = token.NumericValue();
    if (image_scale_factor <= 0)
      return nullptr;
    image_set->Append(*CSSPrimitiveValue::Create(
        image_scale_factor, CSSPrimitiveValue::UnitType::kNumber));
  } while (ConsumeCommaIncludingWhitespace(args));
  if (!args.AtEnd())
    return nullptr;
  range = range_copy;
  return image_set;
}

static bool IsGeneratedImage(CSSValueID id) {
  return id == CSSValueLinearGradient || id == CSSValueRadialGradient ||
         id == CSSValueConicGradient || id == CSSValueRepeatingLinearGradient ||
         id == CSSValueRepeatingRadialGradient ||
         id == CSSValueRepeatingConicGradient ||
         id == CSSValueWebkitLinearGradient ||
         id == CSSValueWebkitRadialGradient ||
         id == CSSValueWebkitRepeatingLinearGradient ||
         id == CSSValueWebkitRepeatingRadialGradient ||
         id == CSSValueWebkitGradient || id == CSSValueWebkitCrossFade ||
         id == CSSValuePaint;
}

CSSValue* ConsumeImage(CSSParserTokenRange& range,
                       const CSSParserContext* context,
                       ConsumeGeneratedImagePolicy generated_image) {
  AtomicString uri = ConsumeUrlAsStringView(range).ToAtomicString();
  if (!uri.IsNull())
    return CreateCSSImageValueWithReferrer(uri, context);
  if (range.Peek().GetType() == kFunctionToken) {
    CSSValueID id = range.Peek().FunctionId();
    if (id == CSSValueWebkitImageSet)
      return ConsumeImageSet(range, context);
    if (generated_image == ConsumeGeneratedImagePolicy::kAllow &&
        IsGeneratedImage(id)) {
      return ConsumeGeneratedImage(range, context);
    }
  }
  return nullptr;
}

// https://drafts.csswg.org/css-values-4/#css-wide-keywords
bool IsCSSWideKeyword(StringView keyword) {
  return EqualIgnoringASCIICase(keyword, "initial") ||
         EqualIgnoringASCIICase(keyword, "inherit") ||
         EqualIgnoringASCIICase(keyword, "unset");
}

// https://drafts.csswg.org/css-shapes-1/#typedef-shape-box
CSSIdentifierValue* ConsumeShapeBox(CSSParserTokenRange& range) {
  return ConsumeIdent<CSSValueContentBox, CSSValuePaddingBox, CSSValueBorderBox,
                      CSSValueMarginBox>(range);
}

void AddProperty(CSSPropertyID resolved_property,
                 CSSPropertyID current_shorthand,
                 const CSSValue& value,
                 bool important,
                 IsImplicitProperty implicit,
                 HeapVector<CSSProperty, 256>& properties) {
  DCHECK(!isPropertyAlias(resolved_property));
  DCHECK(implicit == IsImplicitProperty::kNotImplicit ||
         implicit == IsImplicitProperty::kImplicit);

  int shorthand_index = 0;
  bool set_from_shorthand = false;

  if (current_shorthand) {
    Vector<StylePropertyShorthand, 4> shorthands;
    getMatchingShorthandsForLonghand(resolved_property, &shorthands);
    set_from_shorthand = true;
    if (shorthands.size() > 1) {
      shorthand_index =
          indexOfShorthandForLonghand(current_shorthand, shorthands);
    }
  }

  properties.push_back(CSSProperty(resolved_property, value, important,
                                   set_from_shorthand, shorthand_index,
                                   implicit == IsImplicitProperty::kImplicit));
}

CSSValue* ConsumeTransformList(CSSParserTokenRange& range,
                               const CSSParserContext& context) {
  return CSSPropertyTransformUtils::ConsumeTransformList(
      range, context, CSSParserLocalContext());
}

CSSValue* ConsumeFilterFunctionList(CSSParserTokenRange& range,
                                    const CSSParserContext& context) {
  if (range.Peek().Id() == CSSValueNone)
    return ConsumeIdent(range);

  CSSValueList* list = CSSValueList::CreateSpaceSeparated();
  do {
    CSSValue* filter_value = ConsumeUrl(range, &context);
    if (!filter_value) {
      filter_value = ConsumeFilterFunction(range, context);
      if (!filter_value)
        return nullptr;
    }
    list->Append(*filter_value);
  } while (!range.AtEnd());
  return list;
}

void CountKeywordOnlyPropertyUsage(CSSPropertyID property,
                                   const CSSParserContext& context,
                                   CSSValueID value_id) {
  if (!context.IsUseCounterRecordingEnabled())
    return;
  switch (property) {
    case CSSPropertyWebkitAppearance: {
      WebFeature feature;
      if (value_id == CSSValueNone) {
        feature = WebFeature::kCSSValueAppearanceNone;
      } else {
        feature = WebFeature::kCSSValueAppearanceNotNone;
        if (value_id == CSSValueButton)
          feature = WebFeature::kCSSValueAppearanceButton;
        else if (value_id == CSSValueCaret)
          feature = WebFeature::kCSSValueAppearanceCaret;
        else if (value_id == CSSValueCheckbox)
          feature = WebFeature::kCSSValueAppearanceCheckbox;
        else if (value_id == CSSValueMenulist)
          feature = WebFeature::kCSSValueAppearanceMenulist;
        else if (value_id == CSSValueMenulistButton)
          feature = WebFeature::kCSSValueAppearanceMenulistButton;
        else if (value_id == CSSValueListbox)
          feature = WebFeature::kCSSValueAppearanceListbox;
        else if (value_id == CSSValueRadio)
          feature = WebFeature::kCSSValueAppearanceRadio;
        else if (value_id == CSSValueSearchfield)
          feature = WebFeature::kCSSValueAppearanceSearchField;
        else if (value_id == CSSValueTextfield)
          feature = WebFeature::kCSSValueAppearanceTextField;
        else
          feature = WebFeature::kCSSValueAppearanceOthers;
      }
      context.Count(feature);
      break;
    }

    case CSSPropertyWebkitUserModify: {
      switch (value_id) {
        case CSSValueReadOnly:
          context.Count(WebFeature::kCSSValueUserModifyReadOnly);
          break;
        case CSSValueReadWrite:
          context.Count(WebFeature::kCSSValueUserModifyReadWrite);
          break;
        case CSSValueReadWritePlaintextOnly:
          context.Count(WebFeature::kCSSValueUserModifyReadWritePlaintextOnly);
          break;
        default:
          NOTREACHED();
      }
      break;
    }

    default:
      break;
  }
}

const CSSValue* ParseLonghandViaAPI(CSSPropertyID unresolved_property,
                                    CSSPropertyID current_shorthand,
                                    const CSSParserContext& context,
                                    CSSParserTokenRange& range) {
  DCHECK(!isShorthandProperty(unresolved_property));
  CSSPropertyID property = resolveCSSPropertyID(unresolved_property);
  if (CSSParserFastPaths::IsKeywordPropertyID(property)) {
    if (!CSSParserFastPaths::IsValidKeywordPropertyAndValue(
            property, range.Peek().Id(), context.Mode()))
      return nullptr;
    CountKeywordOnlyPropertyUsage(property, context, range.Peek().Id());
    return ConsumeIdent(range);
  }

  const CSSPropertyAPI& api = CSSPropertyAPI::Get(property);
  const CSSValue* result = api.ParseSingleValue(
      range, context,
      CSSParserLocalContext(isPropertyAlias(unresolved_property),
                            current_shorthand));
  return result;
}

bool ConsumeShorthandVia2LonghandAPIs(
    const StylePropertyShorthand& shorthand,
    bool important,
    const CSSParserContext& context,
    CSSParserTokenRange& range,
    HeapVector<CSSProperty, 256>& properties) {
  DCHECK_EQ(shorthand.length(), 2u);
  const CSSPropertyID* longhands = shorthand.properties();

  const CSSValue* start =
      ParseLonghandViaAPI(longhands[0], shorthand.id(), context, range);

  if (!start)
    return false;

  const CSSValue* end =
      ParseLonghandViaAPI(longhands[1], shorthand.id(), context, range);

  if (!end)
    end = start;
  AddProperty(longhands[0], shorthand.id(), *start, important,
              IsImplicitProperty::kNotImplicit, properties);
  AddProperty(longhands[1], shorthand.id(), *end, important,
              IsImplicitProperty::kNotImplicit, properties);

  return range.AtEnd();
}

bool ConsumeShorthandVia4LonghandAPIs(
    const StylePropertyShorthand& shorthand,
    bool important,
    const CSSParserContext& context,
    CSSParserTokenRange& range,
    HeapVector<CSSProperty, 256>& properties) {
  DCHECK_EQ(shorthand.length(), 4u);
  const CSSPropertyID* longhands = shorthand.properties();
  const CSSValue* top =
      ParseLonghandViaAPI(longhands[0], shorthand.id(), context, range);

  if (!top)
    return false;

  const CSSValue* right =
      ParseLonghandViaAPI(longhands[1], shorthand.id(), context, range);

  const CSSValue* bottom = nullptr;
  const CSSValue* left = nullptr;
  if (right) {
    bottom = ParseLonghandViaAPI(longhands[2], shorthand.id(), context, range);
    if (bottom) {
      left = ParseLonghandViaAPI(longhands[3], shorthand.id(), context, range);
    }
  }

  if (!right)
    right = top;
  if (!bottom)
    bottom = top;
  if (!left)
    left = right;

  AddProperty(longhands[0], shorthand.id(), *top, important,
              IsImplicitProperty::kNotImplicit, properties);
  AddProperty(longhands[1], shorthand.id(), *right, important,
              IsImplicitProperty::kNotImplicit, properties);
  AddProperty(longhands[2], shorthand.id(), *bottom, important,
              IsImplicitProperty::kNotImplicit, properties);
  AddProperty(longhands[3], shorthand.id(), *left, important,
              IsImplicitProperty::kNotImplicit, properties);

  return range.AtEnd();
}

bool ConsumeShorthandGreedilyViaLonghandAPIs(
    const StylePropertyShorthand& shorthand,
    bool important,
    const CSSParserContext& context,
    CSSParserTokenRange& range,
    HeapVector<CSSProperty, 256>& properties) {
  // Existing shorthands have at most 6 longhands.
  DCHECK_LE(shorthand.length(), 6u);
  const CSSValue* longhands[6] = {nullptr, nullptr, nullptr,
                                  nullptr, nullptr, nullptr};
  const CSSPropertyID* shorthand_properties = shorthand.properties();
  do {
    bool found_longhand = false;
    for (size_t i = 0; !found_longhand && i < shorthand.length(); ++i) {
      if (longhands[i])
        continue;
      longhands[i] = ParseLonghandViaAPI(shorthand_properties[i],
                                         shorthand.id(), context, range);

      if (longhands[i])
        found_longhand = true;
    }
    if (!found_longhand)
      return false;
  } while (!range.AtEnd());

  for (size_t i = 0; i < shorthand.length(); ++i) {
    if (longhands[i]) {
      AddProperty(shorthand_properties[i], shorthand.id(), *longhands[i],
                  important, IsImplicitProperty::kNotImplicit, properties);
    } else {
      AddProperty(shorthand_properties[i], shorthand.id(),
                  *CSSInitialValue::Create(), important,
                  IsImplicitProperty::kNotImplicit, properties);
    }
  }
  return true;
}

void AddExpandedPropertyForValue(CSSPropertyID property,
                                 const CSSValue& value,
                                 bool important,
                                 HeapVector<CSSProperty, 256>& properties) {
  const StylePropertyShorthand& shorthand = shorthandForProperty(property);
  unsigned shorthand_length = shorthand.length();
  DCHECK(shorthand_length);
  const CSSPropertyID* longhands = shorthand.properties();
  for (unsigned i = 0; i < shorthand_length; ++i) {
    AddProperty(longhands[i], property, value, important,
                IsImplicitProperty::kNotImplicit, properties);
  }
}

}  // namespace CSSPropertyParserHelpers

}  // namespace blink

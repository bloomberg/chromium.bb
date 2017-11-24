/*
 * CSS Media Query
 *
 * Copyright (C) 2006 Kimmo Kinnunen <kimmo.t.kinnunen@nokia.com>.
 * Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies).
 * Copyright (C) 2013 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "core/css/MediaQueryExp.h"

#include "core/css/parser/CSSParserToken.h"
#include "core/html/parser/HTMLParserIdioms.h"
#include "platform/Decimal.h"
#include "platform/runtime_enabled_features.h"
#include "platform/wtf/text/StringBuffer.h"
#include "platform/wtf/text/StringBuilder.h"

namespace blink {

using namespace MediaFeatureNames;

static inline bool FeatureWithValidIdent(const String& media_feature,
                                         CSSValueID ident) {
  if (media_feature == displayModeMediaFeature)
    return ident == CSSValueFullscreen || ident == CSSValueStandalone ||
           ident == CSSValueMinimalUi || ident == CSSValueBrowser;

  if (media_feature == orientationMediaFeature)
    return ident == CSSValuePortrait || ident == CSSValueLandscape;

  if (media_feature == pointerMediaFeature ||
      media_feature == anyPointerMediaFeature)
    return ident == CSSValueNone || ident == CSSValueCoarse ||
           ident == CSSValueFine;

  if (media_feature == hoverMediaFeature ||
      media_feature == anyHoverMediaFeature)
    return ident == CSSValueNone || ident == CSSValueHover;

  if (media_feature == scanMediaFeature)
    return ident == CSSValueInterlace || ident == CSSValueProgressive;

  if (RuntimeEnabledFeatures::MediaQueryShapeEnabled()) {
    if (media_feature == shapeMediaFeature)
      return ident == CSSValueRect || ident == CSSValueRound;
  }

  if (media_feature == colorGamutMediaFeature) {
    return ident == CSSValueSRGB || ident == CSSValueP3 ||
           ident == CSSValueRec2020;
  }

  return false;
}

static inline bool FeatureWithValidPositiveLength(const String& media_feature,
                                                  const CSSParserToken& token) {
  if (!(CSSPrimitiveValue::IsLength(token.GetUnitType()) ||
        (token.GetType() == kNumberToken && token.NumericValue() == 0)) ||
      token.NumericValue() < 0)
    return false;

  return media_feature == heightMediaFeature ||
         media_feature == maxHeightMediaFeature ||
         media_feature == minHeightMediaFeature ||
         media_feature == widthMediaFeature ||
         media_feature == maxWidthMediaFeature ||
         media_feature == minWidthMediaFeature ||
         media_feature == deviceHeightMediaFeature ||
         media_feature == maxDeviceHeightMediaFeature ||
         media_feature == minDeviceHeightMediaFeature ||
         media_feature == deviceWidthMediaFeature ||
         media_feature == minDeviceWidthMediaFeature ||
         media_feature == maxDeviceWidthMediaFeature;
}

static inline bool FeatureWithValidDensity(const String& media_feature,
                                           const CSSParserToken& token) {
  if ((token.GetUnitType() != CSSPrimitiveValue::UnitType::kDotsPerPixel &&
       token.GetUnitType() != CSSPrimitiveValue::UnitType::kDotsPerInch &&
       token.GetUnitType() !=
           CSSPrimitiveValue::UnitType::kDotsPerCentimeter) ||
      token.NumericValue() <= 0)
    return false;

  return media_feature == resolutionMediaFeature ||
         media_feature == minResolutionMediaFeature ||
         media_feature == maxResolutionMediaFeature;
}

static inline bool FeatureWithPositiveInteger(const String& media_feature,
                                              const CSSParserToken& token) {
  if (token.GetNumericValueType() != kIntegerValueType ||
      token.NumericValue() < 0)
    return false;

  return media_feature == colorMediaFeature ||
         media_feature == maxColorMediaFeature ||
         media_feature == minColorMediaFeature ||
         media_feature == colorIndexMediaFeature ||
         media_feature == maxColorIndexMediaFeature ||
         media_feature == minColorIndexMediaFeature ||
         media_feature == monochromeMediaFeature ||
         media_feature == maxMonochromeMediaFeature ||
         media_feature == minMonochromeMediaFeature;
}

static inline bool FeatureWithPositiveNumber(const String& media_feature,
                                             const CSSParserToken& token) {
  if (token.GetType() != kNumberToken || token.NumericValue() < 0)
    return false;

  return media_feature == transform3dMediaFeature ||
         media_feature == devicePixelRatioMediaFeature ||
         media_feature == maxDevicePixelRatioMediaFeature ||
         media_feature == minDevicePixelRatioMediaFeature;
}

static inline bool FeatureWithZeroOrOne(const String& media_feature,
                                        const CSSParserToken& token) {
  if (token.GetNumericValueType() != kIntegerValueType ||
      !(token.NumericValue() == 1 || !token.NumericValue()))
    return false;

  return media_feature == gridMediaFeature;
}

static inline bool FeatureWithAspectRatio(const String& media_feature) {
  return media_feature == aspectRatioMediaFeature ||
         media_feature == deviceAspectRatioMediaFeature ||
         media_feature == minAspectRatioMediaFeature ||
         media_feature == maxAspectRatioMediaFeature ||
         media_feature == minDeviceAspectRatioMediaFeature ||
         media_feature == maxDeviceAspectRatioMediaFeature;
}

static inline bool FeatureWithoutValue(const String& media_feature) {
  // Media features that are prefixed by min/max cannot be used without a value.
  return media_feature == monochromeMediaFeature ||
         media_feature == colorMediaFeature ||
         media_feature == colorIndexMediaFeature ||
         media_feature == gridMediaFeature ||
         media_feature == heightMediaFeature ||
         media_feature == widthMediaFeature ||
         media_feature == deviceHeightMediaFeature ||
         media_feature == deviceWidthMediaFeature ||
         media_feature == orientationMediaFeature ||
         media_feature == aspectRatioMediaFeature ||
         media_feature == deviceAspectRatioMediaFeature ||
         media_feature == hoverMediaFeature ||
         media_feature == anyHoverMediaFeature ||
         media_feature == transform3dMediaFeature ||
         media_feature == pointerMediaFeature ||
         media_feature == anyPointerMediaFeature ||
         media_feature == devicePixelRatioMediaFeature ||
         media_feature == resolutionMediaFeature ||
         media_feature == displayModeMediaFeature ||
         media_feature == scanMediaFeature ||
         media_feature == shapeMediaFeature ||
         media_feature == colorGamutMediaFeature;
}

bool MediaQueryExp::IsViewportDependent() const {
  return media_feature_ == widthMediaFeature ||
         media_feature_ == heightMediaFeature ||
         media_feature_ == minWidthMediaFeature ||
         media_feature_ == minHeightMediaFeature ||
         media_feature_ == maxWidthMediaFeature ||
         media_feature_ == maxHeightMediaFeature ||
         media_feature_ == orientationMediaFeature ||
         media_feature_ == aspectRatioMediaFeature ||
         media_feature_ == minAspectRatioMediaFeature ||
         media_feature_ == devicePixelRatioMediaFeature ||
         media_feature_ == resolutionMediaFeature ||
         media_feature_ == maxAspectRatioMediaFeature ||
         media_feature_ == maxDevicePixelRatioMediaFeature ||
         media_feature_ == minDevicePixelRatioMediaFeature;
}

bool MediaQueryExp::IsDeviceDependent() const {
  return media_feature_ == deviceAspectRatioMediaFeature ||
         media_feature_ == deviceWidthMediaFeature ||
         media_feature_ == deviceHeightMediaFeature ||
         media_feature_ == minDeviceAspectRatioMediaFeature ||
         media_feature_ == minDeviceWidthMediaFeature ||
         media_feature_ == minDeviceHeightMediaFeature ||
         media_feature_ == maxDeviceAspectRatioMediaFeature ||
         media_feature_ == maxDeviceWidthMediaFeature ||
         media_feature_ == maxDeviceHeightMediaFeature ||
         media_feature_ == shapeMediaFeature;
}

MediaQueryExp::MediaQueryExp(const MediaQueryExp& other)
    : media_feature_(other.MediaFeature()), exp_value_(other.ExpValue()) {}

MediaQueryExp::MediaQueryExp(const String& media_feature,
                             const MediaQueryExpValue& exp_value)
    : media_feature_(media_feature), exp_value_(exp_value) {}

MediaQueryExp MediaQueryExp::Create(
    const String& media_feature,
    const Vector<CSSParserToken, 4>& token_list) {
  DCHECK(!media_feature.IsNull());

  MediaQueryExpValue exp_value;
  String lower_media_feature =
      AttemptStaticStringCreation(media_feature.LowerASCII());

  // Create value for media query expression that must have 1 or more values.
  if (token_list.size() == 0 && FeatureWithoutValue(lower_media_feature)) {
    // Valid, creates a MediaQueryExp with an 'invalid' MediaQueryExpValue
  } else if (token_list.size() == 1) {
    CSSParserToken token = token_list.front();

    if (token.GetType() == kIdentToken) {
      CSSValueID ident = token.Id();
      if (!FeatureWithValidIdent(lower_media_feature, ident))
        return Invalid();
      exp_value.id = ident;
      exp_value.is_id = true;
    } else if (token.GetType() == kNumberToken ||
               token.GetType() == kPercentageToken ||
               token.GetType() == kDimensionToken) {
      // Check for numeric token types since it is only safe for these types to
      // call numericValue.
      if (FeatureWithValidDensity(lower_media_feature, token) ||
          FeatureWithValidPositiveLength(lower_media_feature, token)) {
        // Media features that must have non-negative <density>, ie. dppx, dpi
        // or dpcm, or Media features that must have non-negative <length> or
        // number value.
        exp_value.value = token.NumericValue();
        exp_value.unit = token.GetUnitType();
        exp_value.is_value = true;
      } else if (FeatureWithPositiveInteger(lower_media_feature, token) ||
                 FeatureWithPositiveNumber(lower_media_feature, token) ||
                 FeatureWithZeroOrOne(lower_media_feature, token)) {
        // Media features that must have non-negative integer value,
        // or media features that must have non-negative number value,
        // or media features that must have (0|1) value.
        exp_value.value = token.NumericValue();
        exp_value.unit = CSSPrimitiveValue::UnitType::kNumber;
        exp_value.is_value = true;
      } else {
        return Invalid();
      }
    } else {
      return Invalid();
    }
  } else if (token_list.size() == 3 &&
             FeatureWithAspectRatio(lower_media_feature)) {
    // TODO(timloh): <ratio> is supposed to allow whitespace around the '/'
    // Applicable to device-aspect-ratio and aspect-ratio.
    const CSSParserToken& numerator = token_list[0];
    const CSSParserToken& delimiter = token_list[1];
    const CSSParserToken& denominator = token_list[2];
    if (delimiter.GetType() != kDelimiterToken || delimiter.Delimiter() != '/')
      return Invalid();
    if (numerator.GetType() != kNumberToken || numerator.NumericValue() <= 0 ||
        numerator.GetNumericValueType() != kIntegerValueType)
      return Invalid();
    if (denominator.GetType() != kNumberToken ||
        denominator.NumericValue() <= 0 ||
        denominator.GetNumericValueType() != kIntegerValueType)
      return Invalid();

    exp_value.numerator = clampTo<unsigned>(numerator.NumericValue());
    exp_value.denominator = clampTo<unsigned>(denominator.NumericValue());
    exp_value.is_ratio = true;
  } else {
    return Invalid();
  }

  return MediaQueryExp(lower_media_feature, exp_value);
}

MediaQueryExp::~MediaQueryExp() = default;

bool MediaQueryExp::operator==(const MediaQueryExp& other) const {
  return (other.media_feature_ == media_feature_) &&
         ((!other.exp_value_.IsValid() && !exp_value_.IsValid()) ||
          (other.exp_value_.IsValid() && exp_value_.IsValid() &&
           other.exp_value_.Equals(exp_value_)));
}

String MediaQueryExp::Serialize() const {
  StringBuilder result;
  result.Append('(');
  result.Append(media_feature_.LowerASCII());
  if (exp_value_.IsValid()) {
    result.Append(": ");
    result.Append(exp_value_.CssText());
  }
  result.Append(')');

  return result.ToString();
}

static inline String PrintNumber(double number) {
  return Decimal::FromDouble(number).ToString();
}

String MediaQueryExpValue::CssText() const {
  StringBuilder output;
  if (is_value) {
    output.Append(PrintNumber(value));
    output.Append(CSSPrimitiveValue::UnitTypeToString(unit));
  } else if (is_ratio) {
    output.Append(PrintNumber(numerator));
    output.Append('/');
    output.Append(PrintNumber(denominator));
  } else if (is_id) {
    output.Append(getValueName(id));
  }

  return output.ToString();
}

}  // namespace blink

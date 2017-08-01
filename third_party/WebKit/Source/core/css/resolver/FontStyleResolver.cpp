// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/resolver/FontStyleResolver.h"

#include "core/css/CSSToLengthConversionData.h"
#include "core/css/resolver/FontBuilder.h"
#include "core/css/resolver/StyleBuilderConverter.h"
#include "platform/fonts/FontDescription.h"

namespace blink {

FontDescription FontStyleResolver::ComputeFont(
    const StylePropertySet& property_set) {
  FontBuilder builder(nullptr);

  FontDescription fontDescription;
  Font font(fontDescription);
  CSSToLengthConversionData::FontSizes fontSizes(16, 16, &font);
  CSSToLengthConversionData::ViewportSize viewportSize(0, 0);
  CSSToLengthConversionData conversionData(nullptr, fontSizes, viewportSize, 1);

  // CSSPropertyFontSize
  builder.SetSize(StyleBuilderConverterBase::ConvertFontSize(
      *property_set.GetPropertyCSSValue(CSSPropertyFontSize), conversionData,
      FontDescription::Size(0, 0.0f, false)));

  // CSSPropertyFontFamily
  builder.SetFamilyDescription(StyleBuilderConverterBase::ConvertFontFamily(
      *property_set.GetPropertyCSSValue(CSSPropertyFontFamily), &builder,
      nullptr));

  // CSSPropertyFontStretch
  builder.SetStretch(StyleBuilderConverterBase::ConvertFontStretch(
      *property_set.GetPropertyCSSValue(CSSPropertyFontStretch)));

  // CSSPropertyFontStyle
  builder.SetStyle(ToCSSIdentifierValue(
                       *property_set.GetPropertyCSSValue(CSSPropertyFontStyle))
                       .ConvertTo<FontSelectionValueStyle>());

  // CSSPropertyFontVariantCaps
  builder.SetVariantCaps(StyleBuilderConverterBase::ConvertFontVariantCaps(
      *property_set.GetPropertyCSSValue(CSSPropertyFontVariantCaps)));

  // CSSPropertyFontWeight
  builder.SetWeight(StyleBuilderConverterBase::ConvertFontWeight(
      *property_set.GetPropertyCSSValue(CSSPropertyFontWeight),
      FontBuilder::InitialWeight()));

  builder.UpdateFontDescription(fontDescription);

  return fontDescription;
}

}  // namespace blink

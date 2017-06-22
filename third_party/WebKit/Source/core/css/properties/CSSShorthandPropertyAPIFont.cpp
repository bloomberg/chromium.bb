// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/properties/CSSShorthandPropertyAPIFont.h"

#include "core/css/CSSFontFamilyValue.h"
#include "core/css/CSSIdentifierValue.h"
#include "core/css/CSSPrimitiveValueMappings.h"
#include "core/css/CSSProperty.h"
#include "core/css/parser/CSSParserContext.h"
#include "core/css/parser/CSSParserFastPaths.h"
#include "core/css/parser/CSSPropertyParserHelpers.h"
#include "core/css/properties/CSSPropertyFontUtils.h"
#include "core/layout/LayoutTheme.h"
#include "platform/fonts/FontTraits.h"

namespace blink {

namespace {

bool ConsumeSystemFont(bool important,
                       CSSParserTokenRange& range,
                       HeapVector<CSSProperty, 256>& properties) {
  CSSValueID system_font_id = range.ConsumeIncludingWhitespace().Id();
  DCHECK_GE(system_font_id, CSSValueCaption);
  DCHECK_LE(system_font_id, CSSValueStatusBar);
  if (!range.AtEnd())
    return false;

  FontStyle font_style = kFontStyleNormal;
  FontWeight font_weight = kFontWeightNormal;
  float font_size = 0;
  AtomicString font_family;
  LayoutTheme::GetTheme().SystemFont(system_font_id, font_style, font_weight,
                                     font_size, font_family);

  CSSPropertyParserHelpers::AddProperty(
      CSSPropertyFontStyle, CSSPropertyFont,
      *CSSIdentifierValue::Create(
          font_style == kFontStyleItalic ? CSSValueItalic : CSSValueNormal),
      important, CSSPropertyParserHelpers::IsImplicitProperty::kNotImplicit,
      properties);
  CSSPropertyParserHelpers::AddProperty(
      CSSPropertyFontWeight, CSSPropertyFont,
      *CSSIdentifierValue::Create(font_weight), important,
      CSSPropertyParserHelpers::IsImplicitProperty::kNotImplicit, properties);
  CSSPropertyParserHelpers::AddProperty(
      CSSPropertyFontSize, CSSPropertyFont,
      *CSSPrimitiveValue::Create(font_size,
                                 CSSPrimitiveValue::UnitType::kPixels),
      important, CSSPropertyParserHelpers::IsImplicitProperty::kNotImplicit,
      properties);

  CSSValueList* font_family_list = CSSValueList::CreateCommaSeparated();
  font_family_list->Append(*CSSFontFamilyValue::Create(font_family));
  CSSPropertyParserHelpers::AddProperty(
      CSSPropertyFontFamily, CSSPropertyFont, *font_family_list, important,
      CSSPropertyParserHelpers::IsImplicitProperty::kNotImplicit, properties);

  CSSPropertyParserHelpers::AddProperty(
      CSSPropertyFontStretch, CSSPropertyFont,
      *CSSIdentifierValue::Create(CSSValueNormal), important,
      CSSPropertyParserHelpers::IsImplicitProperty::kNotImplicit, properties);
  CSSPropertyParserHelpers::AddProperty(
      CSSPropertyFontVariantCaps, CSSPropertyFont,
      *CSSIdentifierValue::Create(CSSValueNormal), important,
      CSSPropertyParserHelpers::IsImplicitProperty::kNotImplicit, properties);
  CSSPropertyParserHelpers::AddProperty(
      CSSPropertyFontVariantLigatures, CSSPropertyFont,
      *CSSIdentifierValue::Create(CSSValueNormal), important,
      CSSPropertyParserHelpers::IsImplicitProperty::kNotImplicit, properties);
  CSSPropertyParserHelpers::AddProperty(
      CSSPropertyFontVariantNumeric, CSSPropertyFont,
      *CSSIdentifierValue::Create(CSSValueNormal), important,
      CSSPropertyParserHelpers::IsImplicitProperty::kNotImplicit, properties);
  CSSPropertyParserHelpers::AddProperty(
      CSSPropertyLineHeight, CSSPropertyFont,
      *CSSIdentifierValue::Create(CSSValueNormal), important,
      CSSPropertyParserHelpers::IsImplicitProperty::kNotImplicit, properties);
  return true;
}

bool ConsumeFont(bool important,
                 CSSParserTokenRange& range,
                 const CSSParserContext& context,
                 HeapVector<CSSProperty, 256>& properties) {
  // Let's check if there is an inherit or initial somewhere in the shorthand.
  CSSParserTokenRange range_copy = range;
  while (!range_copy.AtEnd()) {
    CSSValueID id = range_copy.ConsumeIncludingWhitespace().Id();
    if (id == CSSValueInherit || id == CSSValueInitial)
      return false;
  }
  // Optional font-style, font-variant, font-stretch and font-weight.
  CSSIdentifierValue* font_style = nullptr;
  CSSIdentifierValue* font_variant_caps = nullptr;
  CSSIdentifierValue* font_weight = nullptr;
  CSSIdentifierValue* font_stretch = nullptr;
  while (!range.AtEnd()) {
    CSSValueID id = range.Peek().Id();
    if (!font_style && CSSParserFastPaths::IsValidKeywordPropertyAndValue(
                           CSSPropertyFontStyle, id, context.Mode())) {
      font_style = CSSPropertyParserHelpers::ConsumeIdent(range);
      continue;
    }
    if (!font_variant_caps &&
        (id == CSSValueNormal || id == CSSValueSmallCaps)) {
      // Font variant in the shorthand is particular, it only accepts normal or
      // small-caps.
      // See https://drafts.csswg.org/css-fonts/#propdef-font
      font_variant_caps = CSSPropertyFontUtils::ConsumeFontVariantCSS21(range);
      if (font_variant_caps)
        continue;
    }
    if (!font_weight) {
      font_weight = CSSPropertyFontUtils::ConsumeFontWeight(range);
      if (font_weight)
        continue;
    }
    if (!font_stretch && CSSParserFastPaths::IsValidKeywordPropertyAndValue(
                             CSSPropertyFontStretch, id, context.Mode()))
      font_stretch = CSSPropertyParserHelpers::ConsumeIdent(range);
    else
      break;
  }

  if (range.AtEnd())
    return false;

  CSSPropertyParserHelpers::AddProperty(
      CSSPropertyFontStyle, CSSPropertyFont,
      font_style ? *font_style : *CSSIdentifierValue::Create(CSSValueNormal),
      important, CSSPropertyParserHelpers::IsImplicitProperty::kNotImplicit,
      properties);
  CSSPropertyParserHelpers::AddProperty(
      CSSPropertyFontVariantCaps, CSSPropertyFont,
      font_variant_caps ? *font_variant_caps
                        : *CSSIdentifierValue::Create(CSSValueNormal),
      important, CSSPropertyParserHelpers::IsImplicitProperty::kNotImplicit,
      properties);
  CSSPropertyParserHelpers::AddProperty(
      CSSPropertyFontVariantLigatures, CSSPropertyFont,
      *CSSIdentifierValue::Create(CSSValueNormal), important,
      CSSPropertyParserHelpers::IsImplicitProperty::kNotImplicit, properties);
  CSSPropertyParserHelpers::AddProperty(
      CSSPropertyFontVariantNumeric, CSSPropertyFont,
      *CSSIdentifierValue::Create(CSSValueNormal), important,
      CSSPropertyParserHelpers::IsImplicitProperty::kNotImplicit, properties);

  CSSPropertyParserHelpers::AddProperty(
      CSSPropertyFontWeight, CSSPropertyFont,
      font_weight ? *font_weight : *CSSIdentifierValue::Create(CSSValueNormal),
      important, CSSPropertyParserHelpers::IsImplicitProperty::kNotImplicit,
      properties);
  CSSPropertyParserHelpers::AddProperty(
      CSSPropertyFontStretch, CSSPropertyFont,
      font_stretch ? *font_stretch
                   : *CSSIdentifierValue::Create(CSSValueNormal),
      important, CSSPropertyParserHelpers::IsImplicitProperty::kNotImplicit,
      properties);

  // Now a font size _must_ come.
  CSSValue* font_size =
      CSSPropertyFontUtils::ConsumeFontSize(range, context.Mode());
  if (!font_size || range.AtEnd())
    return false;

  CSSPropertyParserHelpers::AddProperty(
      CSSPropertyFontSize, CSSPropertyFont, *font_size, important,
      CSSPropertyParserHelpers::IsImplicitProperty::kNotImplicit, properties);

  if (CSSPropertyParserHelpers::ConsumeSlashIncludingWhitespace(range)) {
    CSSValue* line_height =
        CSSPropertyFontUtils::ConsumeLineHeight(range, context.Mode());
    if (!line_height)
      return false;
    CSSPropertyParserHelpers::AddProperty(
        CSSPropertyLineHeight, CSSPropertyFont, *line_height, important,
        CSSPropertyParserHelpers::IsImplicitProperty::kNotImplicit, properties);
  } else {
    CSSPropertyParserHelpers::AddProperty(
        CSSPropertyLineHeight, CSSPropertyFont,
        *CSSIdentifierValue::Create(CSSValueNormal), important,
        CSSPropertyParserHelpers::IsImplicitProperty::kNotImplicit, properties);
  }

  // Font family must come now.
  CSSValue* parsed_family_value =
      CSSPropertyFontUtils::ConsumeFontFamily(range);
  if (!parsed_family_value)
    return false;

  CSSPropertyParserHelpers::AddProperty(
      CSSPropertyFontFamily, CSSPropertyFont, *parsed_family_value, important,
      CSSPropertyParserHelpers::IsImplicitProperty::kNotImplicit, properties);

  // FIXME: http://www.w3.org/TR/2011/WD-css3-fonts-20110324/#font-prop requires
  // that "font-stretch", "font-size-adjust", and "font-kerning" be reset to
  // their initial values but we don't seem to support them at the moment. They
  // should also be added here once implemented.
  return range.AtEnd();
}

}  // namespace

bool CSSShorthandPropertyAPIFont::parseShorthand(
    bool important,
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&,
    HeapVector<CSSProperty, 256>& properties) {
  const CSSParserToken& token = range.Peek();
  if (token.Id() >= CSSValueCaption && token.Id() <= CSSValueStatusBar)
    return ConsumeSystemFont(important, range, properties);
  return ConsumeFont(important, range, context, properties);
}
}  // namespace blink

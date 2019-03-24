// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/properties/shorthands/font.h"

#include "third_party/blink/renderer/core/css/css_font_family_value.h"
#include "third_party/blink/renderer/core/css/css_identifier_value.h"
#include "third_party/blink/renderer/core/css/css_primitive_value_mappings.h"
#include "third_party/blink/renderer/core/css/css_property_value.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_context.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_fast_paths.h"
#include "third_party/blink/renderer/core/css/parser/css_property_parser_helpers.h"
#include "third_party/blink/renderer/core/css/properties/computed_style_utils.h"
#include "third_party/blink/renderer/core/css/properties/css_parsing_utils.h"
#include "third_party/blink/renderer/core/layout/layout_theme.h"
#include "third_party/blink/renderer/core/style/computed_style.h"

namespace blink {
namespace {

bool ConsumeSystemFont(bool important,
                       CSSParserTokenRange& range,
                       HeapVector<CSSPropertyValue, 256>& properties) {
  CSSValueID system_font_id = range.ConsumeIncludingWhitespace().Id();
  DCHECK_GE(system_font_id, CSSValueID::kCaption);
  DCHECK_LE(system_font_id, CSSValueID::kStatusBar);
  if (!range.AtEnd())
    return false;

  FontSelectionValue font_style = NormalSlopeValue();
  FontSelectionValue font_weight = NormalWeightValue();
  float font_size = 0;
  AtomicString font_family;
  LayoutTheme::GetTheme().SystemFont(system_font_id, font_style, font_weight,
                                     font_size, font_family);

  css_property_parser_helpers::AddProperty(
      CSSPropertyID::kFontStyle, CSSPropertyID::kFont,
      *CSSIdentifierValue::Create(font_style == ItalicSlopeValue()
                                      ? CSSValueID::kItalic
                                      : CSSValueID::kNormal),
      important, css_property_parser_helpers::IsImplicitProperty::kNotImplicit,
      properties);
  css_property_parser_helpers::AddProperty(
      CSSPropertyID::kFontWeight, CSSPropertyID::kFont,
      *CSSPrimitiveValue::Create(font_weight,
                                 CSSPrimitiveValue::UnitType::kNumber),
      important, css_property_parser_helpers::IsImplicitProperty::kNotImplicit,
      properties);
  css_property_parser_helpers::AddProperty(
      CSSPropertyID::kFontSize, CSSPropertyID::kFont,
      *CSSPrimitiveValue::Create(font_size,
                                 CSSPrimitiveValue::UnitType::kPixels),
      important, css_property_parser_helpers::IsImplicitProperty::kNotImplicit,
      properties);

  CSSValueList* font_family_list = CSSValueList::CreateCommaSeparated();
  font_family_list->Append(*CSSFontFamilyValue::Create(font_family));
  css_property_parser_helpers::AddProperty(
      CSSPropertyID::kFontFamily, CSSPropertyID::kFont, *font_family_list,
      important, css_property_parser_helpers::IsImplicitProperty::kNotImplicit,
      properties);

  css_property_parser_helpers::AddProperty(
      CSSPropertyID::kFontStretch, CSSPropertyID::kFont,
      *CSSIdentifierValue::Create(CSSValueID::kNormal), important,
      css_property_parser_helpers::IsImplicitProperty::kNotImplicit,
      properties);
  css_property_parser_helpers::AddProperty(
      CSSPropertyID::kFontVariantCaps, CSSPropertyID::kFont,
      *CSSIdentifierValue::Create(CSSValueID::kNormal), important,
      css_property_parser_helpers::IsImplicitProperty::kNotImplicit,
      properties);
  css_property_parser_helpers::AddProperty(
      CSSPropertyID::kFontVariantLigatures, CSSPropertyID::kFont,
      *CSSIdentifierValue::Create(CSSValueID::kNormal), important,
      css_property_parser_helpers::IsImplicitProperty::kNotImplicit,
      properties);
  css_property_parser_helpers::AddProperty(
      CSSPropertyID::kFontVariantNumeric, CSSPropertyID::kFont,
      *CSSIdentifierValue::Create(CSSValueID::kNormal), important,
      css_property_parser_helpers::IsImplicitProperty::kNotImplicit,
      properties);
  css_property_parser_helpers::AddProperty(
      CSSPropertyID::kFontVariantEastAsian, CSSPropertyID::kFont,
      *CSSIdentifierValue::Create(CSSValueID::kNormal), important,
      css_property_parser_helpers::IsImplicitProperty::kNotImplicit,
      properties);
  css_property_parser_helpers::AddProperty(
      CSSPropertyID::kLineHeight, CSSPropertyID::kFont,
      *CSSIdentifierValue::Create(CSSValueID::kNormal), important,
      css_property_parser_helpers::IsImplicitProperty::kNotImplicit,
      properties);
  return true;
}

bool ConsumeFont(bool important,
                 CSSParserTokenRange& range,
                 const CSSParserContext& context,
                 HeapVector<CSSPropertyValue, 256>& properties) {
  // Optional font-style, font-variant, font-stretch and font-weight.
  CSSValue* font_style = nullptr;
  CSSIdentifierValue* font_variant_caps = nullptr;
  CSSValue* font_weight = nullptr;
  CSSValue* font_stretch = nullptr;
  while (!range.AtEnd()) {
    CSSValueID id = range.Peek().Id();
    if (!font_style &&
        (id == CSSValueID::kNormal || id == CSSValueID::kItalic ||
         id == CSSValueID::kOblique)) {
      font_style = css_parsing_utils::ConsumeFontStyle(range, context.Mode());
      if (!font_style)
        return false;
      continue;
    }
    if (!font_variant_caps &&
        (id == CSSValueID::kNormal || id == CSSValueID::kSmallCaps)) {
      // Font variant in the shorthand is particular, it only accepts normal or
      // small-caps.
      // See https://drafts.csswg.org/css-fonts/#propdef-font
      font_variant_caps = css_parsing_utils::ConsumeFontVariantCSS21(range);
      if (font_variant_caps)
        continue;
    }
    if (!font_weight) {
      font_weight = css_parsing_utils::ConsumeFontWeight(range, context.Mode());
      if (font_weight)
        continue;
    }
    // Stretch in the font shorthand can only take the CSS Fonts Level 3
    // keywords, not arbitrary values, compare
    // https://drafts.csswg.org/css-fonts-4/#font-prop
    // Bail out if the last possible property of the set in this loop could not
    // be parsed, this closes the first block of optional values of the font
    // shorthand, compare: [ [ <‘font-style’> || <font-variant-css21> ||
    // <‘font-weight’> || <font-stretch-css3> ]?
    if (font_stretch ||
        !(font_stretch =
              css_parsing_utils::ConsumeFontStretchKeywordOnly(range)))
      break;
  }

  if (range.AtEnd())
    return false;

  css_property_parser_helpers::AddProperty(
      CSSPropertyID::kFontStyle, CSSPropertyID::kFont,
      font_style ? *font_style
                 : *CSSIdentifierValue::Create(CSSValueID::kNormal),
      important, css_property_parser_helpers::IsImplicitProperty::kNotImplicit,
      properties);
  css_property_parser_helpers::AddProperty(
      CSSPropertyID::kFontVariantCaps, CSSPropertyID::kFont,
      font_variant_caps ? *font_variant_caps
                        : *CSSIdentifierValue::Create(CSSValueID::kNormal),
      important, css_property_parser_helpers::IsImplicitProperty::kNotImplicit,
      properties);
  css_property_parser_helpers::AddProperty(
      CSSPropertyID::kFontVariantLigatures, CSSPropertyID::kFont,
      *CSSIdentifierValue::Create(CSSValueID::kNormal), important,
      css_property_parser_helpers::IsImplicitProperty::kNotImplicit,
      properties);
  css_property_parser_helpers::AddProperty(
      CSSPropertyID::kFontVariantNumeric, CSSPropertyID::kFont,
      *CSSIdentifierValue::Create(CSSValueID::kNormal), important,
      css_property_parser_helpers::IsImplicitProperty::kNotImplicit,
      properties);
  css_property_parser_helpers::AddProperty(
      CSSPropertyID::kFontVariantEastAsian, CSSPropertyID::kFont,
      *CSSIdentifierValue::Create(CSSValueID::kNormal), important,
      css_property_parser_helpers::IsImplicitProperty::kNotImplicit,
      properties);

  css_property_parser_helpers::AddProperty(
      CSSPropertyID::kFontWeight, CSSPropertyID::kFont,
      font_weight ? *font_weight
                  : *CSSIdentifierValue::Create(CSSValueID::kNormal),
      important, css_property_parser_helpers::IsImplicitProperty::kNotImplicit,
      properties);
  css_property_parser_helpers::AddProperty(
      CSSPropertyID::kFontStretch, CSSPropertyID::kFont,
      font_stretch ? *font_stretch
                   : *CSSIdentifierValue::Create(CSSValueID::kNormal),
      important, css_property_parser_helpers::IsImplicitProperty::kNotImplicit,
      properties);

  // Now a font size _must_ come.
  CSSValue* font_size =
      css_parsing_utils::ConsumeFontSize(range, context.Mode());
  if (!font_size || range.AtEnd())
    return false;

  css_property_parser_helpers::AddProperty(
      CSSPropertyID::kFontSize, CSSPropertyID::kFont, *font_size, important,
      css_property_parser_helpers::IsImplicitProperty::kNotImplicit,
      properties);

  if (css_property_parser_helpers::ConsumeSlashIncludingWhitespace(range)) {
    CSSValue* line_height =
        css_parsing_utils::ConsumeLineHeight(range, context.Mode());
    if (!line_height)
      return false;
    css_property_parser_helpers::AddProperty(
        CSSPropertyID::kLineHeight, CSSPropertyID::kFont, *line_height,
        important,
        css_property_parser_helpers::IsImplicitProperty::kNotImplicit,
        properties);
  } else {
    css_property_parser_helpers::AddProperty(
        CSSPropertyID::kLineHeight, CSSPropertyID::kFont,
        *CSSIdentifierValue::Create(CSSValueID::kNormal), important,
        css_property_parser_helpers::IsImplicitProperty::kNotImplicit,
        properties);
  }

  // Font family must come now.
  CSSValue* parsed_family_value = css_parsing_utils::ConsumeFontFamily(range);
  if (!parsed_family_value)
    return false;

  css_property_parser_helpers::AddProperty(
      CSSPropertyID::kFontFamily, CSSPropertyID::kFont, *parsed_family_value,
      important, css_property_parser_helpers::IsImplicitProperty::kNotImplicit,
      properties);

  // FIXME: http://www.w3.org/TR/2011/WD-css3-fonts-20110324/#font-prop requires
  // that "font-stretch", "font-size-adjust", and "font-kerning" be reset to
  // their initial values but we don't seem to support them at the moment. They
  // should also be added here once implemented.
  return range.AtEnd();
}

}  // namespace
namespace css_shorthand {

bool Font::ParseShorthand(bool important,
                          CSSParserTokenRange& range,
                          const CSSParserContext& context,
                          const CSSParserLocalContext&,
                          HeapVector<CSSPropertyValue, 256>& properties) const {
  const CSSParserToken& token = range.Peek();
  if (token.Id() >= CSSValueID::kCaption &&
      token.Id() <= CSSValueID::kStatusBar)
    return ConsumeSystemFont(important, range, properties);
  return ConsumeFont(important, range, context, properties);
}

const CSSValue* Font::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle&,
    const LayoutObject*,
    Node* styled_node,
    bool allow_visited_style) const {
  return ComputedStyleUtils::ValueForFont(style);
}

}  // namespace css_shorthand
}  // namespace blink

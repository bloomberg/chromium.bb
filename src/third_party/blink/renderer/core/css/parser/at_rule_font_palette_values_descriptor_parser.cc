// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/parser/at_rule_descriptor_parser.h"

#include "third_party/blink/renderer/core/css/css_string_value.h"
#include "third_party/blink/renderer/core/css/css_value.h"
#include "third_party/blink/renderer/core/css/css_value_pair.h"
#include "third_party/blink/renderer/core/css/parser/at_rule_descriptors.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_context.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_token_range.h"
#include "third_party/blink/renderer/core/css/properties/css_parsing_utils.h"

namespace blink {

namespace {

CSSValue* ConsumeFontFamily(CSSParserTokenRange& range,
                            const CSSParserContext& context) {
  if (CSSValue* string = css_parsing_utils::ConsumeFamilyName(range))
    return string;
  return nullptr;
}

CSSValue* ConsumeBasePalette(CSSParserTokenRange& range,
                             const CSSParserContext& context) {
  if (CSSValue* ident =
          css_parsing_utils::ConsumeIdent<CSSValueID::kLight,
                                          CSSValueID::kDark>(range)) {
    return ident;
  }

  if (CSSValue* palette_index =
          css_parsing_utils::ConsumeInteger(range, context, 0)) {
    return palette_index;
  }

  return nullptr;
}

CSSValue* ConsumeColorOverride(CSSParserTokenRange& range,
                               const CSSParserContext& context) {
  CSSValueList* list = CSSValueList::CreateCommaSeparated();
  do {
    CSSValue* color_index =
        css_parsing_utils::ConsumeInteger(range, context, 0);
    if (!color_index)
      return nullptr;
    range.ConsumeWhitespace();
    CSSValue* color = css_parsing_utils::ConsumeColor(
        range, context, false,
        css_parsing_utils::AllowedColorKeywords::kNoSystemColor);
    if (!color)
      return nullptr;
    list->Append(*MakeGarbageCollected<CSSValuePair>(
        color_index, color, CSSValuePair::kKeepIdenticalValues));
  } while (css_parsing_utils::ConsumeCommaIncludingWhitespace(range));
  if (!range.AtEnd() || !list->length())
    return nullptr;

  return list;
}

}  // namespace

CSSValue* AtRuleDescriptorParser::ParseAtFontPaletteValuesDescriptor(
    AtRuleDescriptorID id,
    CSSParserTokenRange& range,
    const CSSParserContext& context) {
  CSSValue* parsed_value = nullptr;

  switch (id) {
    case AtRuleDescriptorID::FontFamily:
      range.ConsumeWhitespace();
      parsed_value = ConsumeFontFamily(range, context);
      break;
    case AtRuleDescriptorID::BasePalette:
      range.ConsumeWhitespace();
      parsed_value = ConsumeBasePalette(range, context);
      break;
    case AtRuleDescriptorID::OverrideColors:
      range.ConsumeWhitespace();
      parsed_value = ConsumeColorOverride(range, context);
      break;
    default:
      break;
  }

  if (!parsed_value || !range.AtEnd())
    return nullptr;

  return parsed_value;
}

}  // namespace blink

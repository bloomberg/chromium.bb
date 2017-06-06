// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/parser/CSSParser.h"

#include "core/css/CSSColorValue.h"
#include "core/css/CSSKeyframeRule.h"
#include "core/css/StyleColor.h"
#include "core/css/StyleRule.h"
#include "core/css/StyleSheetContents.h"
#include "core/css/parser/CSSParserFastPaths.h"
#include "core/css/parser/CSSParserImpl.h"
#include "core/css/parser/CSSPropertyParser.h"
#include "core/css/parser/CSSSelectorParser.h"
#include "core/css/parser/CSSSupportsParser.h"
#include "core/css/parser/CSSTokenizer.h"
#include "core/css/parser/CSSVariableParser.h"
#include "core/layout/LayoutTheme.h"
#include <memory>

namespace blink {

using namespace cssvalue;

bool CSSParser::ParseDeclarationList(const CSSParserContext* context,
                                     MutableStylePropertySet* property_set,
                                     const String& declaration) {
  return CSSParserImpl::ParseDeclarationList(property_set, declaration,
                                             context);
}

void CSSParser::ParseDeclarationListForInspector(
    const CSSParserContext* context,
    const String& declaration,
    CSSParserObserver& observer) {
  CSSParserImpl::ParseDeclarationListForInspector(declaration, context,
                                                  observer);
}

CSSSelectorList CSSParser::ParseSelector(
    const CSSParserContext* context,
    StyleSheetContents* style_sheet_contents,
    const String& selector) {
  CSSTokenizer tokenizer(selector);
  return CSSSelectorParser::ParseSelector(tokenizer.TokenRange(), context,
                                          style_sheet_contents);
}

CSSSelectorList CSSParser::ParsePageSelector(
    const CSSParserContext* context,
    StyleSheetContents* style_sheet_contents,
    const String& selector) {
  CSSTokenizer tokenizer(selector);
  return CSSParserImpl::ParsePageSelector(tokenizer.TokenRange(),
                                          style_sheet_contents);
}

StyleRuleBase* CSSParser::ParseRule(const CSSParserContext* context,
                                    StyleSheetContents* style_sheet,
                                    const String& rule) {
  return CSSParserImpl::ParseRule(rule, context, style_sheet,
                                  CSSParserImpl::kAllowImportRules);
}

void CSSParser::ParseSheet(const CSSParserContext* context,
                           StyleSheetContents* style_sheet,
                           const String& text,
                           bool defer_property_parsing) {
  return CSSParserImpl::ParseStyleSheet(text, context, style_sheet,
                                        defer_property_parsing);
}

void CSSParser::ParseSheetForInspector(const CSSParserContext* context,
                                       StyleSheetContents* style_sheet,
                                       const String& text,
                                       CSSParserObserver& observer) {
  return CSSParserImpl::ParseStyleSheetForInspector(text, context, style_sheet,
                                                    observer);
}

MutableStylePropertySet::SetResult CSSParser::ParseValue(
    MutableStylePropertySet* declaration,
    CSSPropertyID unresolved_property,
    const String& string,
    bool important) {
  return ParseValue(declaration, unresolved_property, string, important,
                    static_cast<StyleSheetContents*>(nullptr));
}

MutableStylePropertySet::SetResult CSSParser::ParseValue(
    MutableStylePropertySet* declaration,
    CSSPropertyID unresolved_property,
    const String& string,
    bool important,
    StyleSheetContents* style_sheet) {
  if (string.IsEmpty()) {
    bool did_parse = false;
    bool did_change = false;
    return MutableStylePropertySet::SetResult{did_parse, did_change};
  }

  CSSPropertyID resolved_property = resolveCSSPropertyID(unresolved_property);
  CSSParserMode parser_mode = declaration->CssParserMode();
  CSSValue* value = CSSParserFastPaths::MaybeParseValue(resolved_property,
                                                        string, parser_mode);
  if (value) {
    bool did_parse = true;
    bool did_change = declaration->SetProperty(
        CSSProperty(resolved_property, *value, important));
    return MutableStylePropertySet::SetResult{did_parse, did_change};
  }
  CSSParserContext* context;
  if (style_sheet) {
    context = CSSParserContext::Create(style_sheet->ParserContext(), nullptr);
    context->SetMode(parser_mode);
  } else {
    context = CSSParserContext::Create(parser_mode);
  }
  return ParseValue(declaration, unresolved_property, string, important,
                    context);
}

MutableStylePropertySet::SetResult CSSParser::ParseValueForCustomProperty(
    MutableStylePropertySet* declaration,
    const AtomicString& property_name,
    const PropertyRegistry* registry,
    const String& value,
    bool important,
    StyleSheetContents* style_sheet,
    bool is_animation_tainted) {
  DCHECK(CSSVariableParser::IsValidVariableName(property_name));
  if (value.IsEmpty()) {
    bool did_parse = false;
    bool did_change = false;
    return MutableStylePropertySet::SetResult{did_parse, did_change};
  }
  CSSParserMode parser_mode = declaration->CssParserMode();
  CSSParserContext* context;
  if (style_sheet) {
    context = CSSParserContext::Create(style_sheet->ParserContext(), nullptr);
    context->SetMode(parser_mode);
  } else {
    context = CSSParserContext::Create(parser_mode);
  }
  return CSSParserImpl::ParseVariableValue(declaration, property_name, registry,
                                           value, important, context,
                                           is_animation_tainted);
}

ImmutableStylePropertySet* CSSParser::ParseCustomPropertySet(
    CSSParserTokenRange range) {
  return CSSParserImpl::ParseCustomPropertySet(range);
}

MutableStylePropertySet::SetResult CSSParser::ParseValue(
    MutableStylePropertySet* declaration,
    CSSPropertyID unresolved_property,
    const String& string,
    bool important,
    const CSSParserContext* context) {
  return CSSParserImpl::ParseValue(declaration, unresolved_property, string,
                                   important, context);
}

const CSSValue* CSSParser::ParseSingleValue(CSSPropertyID property_id,
                                            const String& string,
                                            const CSSParserContext* context) {
  if (string.IsEmpty())
    return nullptr;
  if (CSSValue* value = CSSParserFastPaths::MaybeParseValue(property_id, string,
                                                            context->Mode()))
    return value;
  CSSTokenizer tokenizer(string);
  return CSSPropertyParser::ParseSingleValue(property_id,
                                             tokenizer.TokenRange(), context);
}

ImmutableStylePropertySet* CSSParser::ParseInlineStyleDeclaration(
    const String& style_string,
    Element* element) {
  return CSSParserImpl::ParseInlineStyleDeclaration(style_string, element);
}

std::unique_ptr<Vector<double>> CSSParser::ParseKeyframeKeyList(
    const String& key_list) {
  return CSSParserImpl::ParseKeyframeKeyList(key_list);
}

StyleRuleKeyframe* CSSParser::ParseKeyframeRule(const CSSParserContext* context,
                                                const String& rule) {
  StyleRuleBase* keyframe = CSSParserImpl::ParseRule(
      rule, context, nullptr, CSSParserImpl::kKeyframeRules);
  return ToStyleRuleKeyframe(keyframe);
}

bool CSSParser::ParseSupportsCondition(const String& condition) {
  CSSTokenizer tokenizer(condition);
  CSSParserImpl parser(StrictCSSParserContext());
  return CSSSupportsParser::SupportsCondition(
             tokenizer.TokenRange(), parser,
             CSSSupportsParser::kForWindowCSS) == CSSSupportsParser::kSupported;
}

bool CSSParser::ParseColor(Color& color, const String& string, bool strict) {
  if (string.IsEmpty())
    return false;

  // The regular color parsers don't resolve named colors, so explicitly
  // handle these first.
  Color named_color;
  if (named_color.SetNamedColor(string)) {
    color = named_color;
    return true;
  }

  const CSSValue* value = CSSParserFastPaths::ParseColor(
      string, strict ? kHTMLStandardMode : kHTMLQuirksMode);
  // TODO(timloh): Why is this always strict mode?
  if (!value)
    value =
        ParseSingleValue(CSSPropertyColor, string, StrictCSSParserContext());

  if (!value || !value->IsColorValue())
    return false;
  color = ToCSSColorValue(*value).Value();
  return true;
}

bool CSSParser::ParseSystemColor(Color& color, const String& color_string) {
  CSSValueID id = CssValueKeywordID(color_string);
  if (!StyleColor::IsSystemColor(id))
    return false;

  color = LayoutTheme::GetTheme().SystemColor(id);
  return true;
}

const CSSValue* CSSParser::ParseFontFaceDescriptor(
    CSSPropertyID property_id,
    const String& property_value,
    const CSSParserContext* context) {
  StringBuilder builder;
  builder.Append("@font-face { ");
  builder.Append(getPropertyNameString(property_id));
  builder.Append(" : ");
  builder.Append(property_value);
  builder.Append("; }");
  StyleRuleBase* rule = ParseRule(context, nullptr, builder.ToString());
  if (!rule || !rule->IsFontFaceRule())
    return nullptr;
  return ToStyleRuleFontFace(rule)->Properties().GetPropertyCSSValue(
      property_id);
}

}  // namespace blink

// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/css_syntax_descriptor.h"

#include <utility>
#include "third_party/blink/renderer/core/css/css_custom_property_declaration.h"
#include "third_party/blink/renderer/core/css/css_syntax_component.h"
#include "third_party/blink/renderer/core/css/css_uri_value.h"
#include "third_party/blink/renderer/core/css/css_value_list.h"
#include "third_party/blink/renderer/core/css/css_variable_reference_value.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_idioms.h"
#include "third_party/blink/renderer/core/css/parser/css_property_parser_helpers.h"
#include "third_party/blink/renderer/core/css/parser/css_variable_parser.h"
#include "third_party/blink/renderer/core/html/parser/html_parser_idioms.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

namespace blink {

const CSSValue* ConsumeSingleType(const CSSSyntaxComponent& syntax,
                                  CSSParserTokenRange& range,
                                  const CSSParserContext* context) {
  using namespace css_property_parser_helpers;

  switch (syntax.GetType()) {
    case CSSSyntaxType::kIdent:
      if (range.Peek().GetType() == kIdentToken &&
          range.Peek().Value() == syntax.GetString()) {
        range.ConsumeIncludingWhitespace();
        return CSSCustomIdentValue::Create(AtomicString(syntax.GetString()));
      }
      return nullptr;
    case CSSSyntaxType::kLength:
      return ConsumeLength(range, kHTMLStandardMode,
                           ValueRange::kValueRangeAll);
    case CSSSyntaxType::kNumber:
      return ConsumeNumber(range, ValueRange::kValueRangeAll);
    case CSSSyntaxType::kPercentage:
      return ConsumePercent(range, ValueRange::kValueRangeAll);
    case CSSSyntaxType::kLengthPercentage:
      return ConsumeLengthOrPercent(range, kHTMLStandardMode,
                                    ValueRange::kValueRangeAll);
    case CSSSyntaxType::kColor:
      return ConsumeColor(range, kHTMLStandardMode);
    case CSSSyntaxType::kImage:
      return ConsumeImage(range, context);
    case CSSSyntaxType::kUrl:
      return ConsumeUrl(range, context);
    case CSSSyntaxType::kInteger:
      return ConsumeIntegerOrNumberCalc(range);
    case CSSSyntaxType::kAngle:
      return ConsumeAngle(range, context, base::Optional<WebFeature>());
    case CSSSyntaxType::kTime:
      return ConsumeTime(range, ValueRange::kValueRangeAll);
    case CSSSyntaxType::kResolution:
      return ConsumeResolution(range);
    case CSSSyntaxType::kTransformFunction:
      return ConsumeTransformValue(range, *context);
    case CSSSyntaxType::kTransformList:
      return ConsumeTransformList(range, *context);
    case CSSSyntaxType::kCustomIdent:
      return ConsumeCustomIdent(range, *context);
    default:
      NOTREACHED();
      return nullptr;
  }
}

const CSSValue* ConsumeSyntaxComponent(const CSSSyntaxComponent& syntax,
                                       CSSParserTokenRange range,
                                       const CSSParserContext* context) {
  // CSS-wide keywords are already handled by the CSSPropertyParser
  if (syntax.GetRepeat() == CSSSyntaxRepeat::kSpaceSeparated) {
    CSSValueList* list = CSSValueList::CreateSpaceSeparated();
    while (!range.AtEnd()) {
      const CSSValue* value = ConsumeSingleType(syntax, range, context);
      if (!value)
        return nullptr;
      list->Append(*value);
    }
    return list->length() ? list : nullptr;
  }
  if (syntax.GetRepeat() == CSSSyntaxRepeat::kCommaSeparated) {
    CSSValueList* list = CSSValueList::CreateCommaSeparated();
    do {
      const CSSValue* value = ConsumeSingleType(syntax, range, context);
      if (!value)
        return nullptr;
      list->Append(*value);
    } while (
        css_property_parser_helpers::ConsumeCommaIncludingWhitespace(range));
    return list->length() ? list : nullptr;
  }
  const CSSValue* result = ConsumeSingleType(syntax, range, context);
  if (!range.AtEnd())
    return nullptr;
  return result;
}

const CSSSyntaxComponent* CSSSyntaxDescriptor::Match(
    const CSSStyleValue& value) const {
  for (const CSSSyntaxComponent& component : syntax_components_) {
    if (component.CanTake(value))
      return &component;
  }
  return nullptr;
}

bool CSSSyntaxDescriptor::CanTake(const CSSStyleValue& value) const {
  return Match(value);
}

const CSSValue* CSSSyntaxDescriptor::Parse(CSSParserTokenRange range,
                                           const CSSParserContext* context,
                                           bool is_animation_tainted) const {
  if (IsTokenStream()) {
    return CSSVariableParser::ParseRegisteredPropertyValue(
        range, *context, false, is_animation_tainted);
  }
  range.ConsumeWhitespace();
  for (const CSSSyntaxComponent& component : syntax_components_) {
    if (const CSSValue* result =
            ConsumeSyntaxComponent(component, range, context))
      return result;
  }
  return CSSVariableParser::ParseRegisteredPropertyValue(range, *context, true,
                                                         is_animation_tainted);
}

CSSSyntaxDescriptor::CSSSyntaxDescriptor(Vector<CSSSyntaxComponent> components)
    : syntax_components_(std::move(components)) {
  DCHECK(syntax_components_.size());
}

CSSSyntaxDescriptor CSSSyntaxDescriptor::CreateUniversal() {
  Vector<CSSSyntaxComponent> components;
  components.push_back(CSSSyntaxComponent(
      CSSSyntaxType::kTokenStream, g_empty_string, CSSSyntaxRepeat::kNone));
  return CSSSyntaxDescriptor(std::move(components));
}

}  // namespace blink

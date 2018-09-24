// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/css_syntax_descriptor.h"

#include "third_party/blink/renderer/core/css/css_custom_property_declaration.h"
#include "third_party/blink/renderer/core/css/css_uri_value.h"
#include "third_party/blink/renderer/core/css/css_value_list.h"
#include "third_party/blink/renderer/core/css/css_variable_reference_value.h"
#include "third_party/blink/renderer/core/css/cssom/css_keyword_value.h"
#include "third_party/blink/renderer/core/css/cssom/css_style_value.h"
#include "third_party/blink/renderer/core/css/cssom/cssom_types.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_idioms.h"
#include "third_party/blink/renderer/core/css/parser/css_property_parser_helpers.h"
#include "third_party/blink/renderer/core/css/parser/css_variable_parser.h"
#include "third_party/blink/renderer/core/html/parser/html_parser_idioms.h"

namespace blink {

void ConsumeWhitespace(const String& string, wtf_size_t& offset) {
  while (IsHTMLSpace(string[offset]))
    offset++;
}

bool ConsumeCharacterAndWhitespace(const String& string,
                                   char character,
                                   wtf_size_t& offset) {
  if (string[offset] != character)
    return false;
  offset++;
  ConsumeWhitespace(string, offset);
  return true;
}

CSSSyntaxType ParseSyntaxType(String type) {
  // TODO(timloh): Are these supposed to be case sensitive?
  if (type == "length")
    return CSSSyntaxType::kLength;
  if (type == "number")
    return CSSSyntaxType::kNumber;
  if (type == "percentage")
    return CSSSyntaxType::kPercentage;
  if (type == "length-percentage")
    return CSSSyntaxType::kLengthPercentage;
  if (type == "color")
    return CSSSyntaxType::kColor;
  if (type == "image")
    return CSSSyntaxType::kImage;
  if (type == "url")
    return CSSSyntaxType::kUrl;
  if (type == "integer")
    return CSSSyntaxType::kInteger;
  if (type == "angle")
    return CSSSyntaxType::kAngle;
  if (type == "time")
    return CSSSyntaxType::kTime;
  if (type == "resolution")
    return CSSSyntaxType::kResolution;
  if (type == "transform-function")
    return CSSSyntaxType::kTransformFunction;
  if (type == "transform-list")
    return CSSSyntaxType::kTransformList;
  if (type == "custom-ident")
    return CSSSyntaxType::kCustomIdent;
  // Not an Ident, just used to indicate failure
  return CSSSyntaxType::kIdent;
}

bool ConsumeSyntaxType(const String& input,
                       wtf_size_t& offset,
                       CSSSyntaxType& type) {
  DCHECK_EQ(input[offset], '<');
  offset++;
  wtf_size_t type_start = offset;
  while (offset < input.length() && input[offset] != '>')
    offset++;
  if (offset == input.length())
    return false;
  type = ParseSyntaxType(input.Substring(type_start, offset - type_start));
  if (type == CSSSyntaxType::kIdent)
    return false;
  offset++;
  return true;
}

bool ConsumeSyntaxIdent(const String& input,
                        wtf_size_t& offset,
                        String& ident) {
  wtf_size_t ident_start = offset;
  while (IsNameCodePoint(input[offset]))
    offset++;
  if (offset == ident_start)
    return false;
  ident = input.Substring(ident_start, offset - ident_start);
  return !CSSPropertyParserHelpers::IsCSSWideKeyword(ident);
}

CSSSyntaxDescriptor::CSSSyntaxDescriptor(const String& input) {
  wtf_size_t offset = 0;
  ConsumeWhitespace(input, offset);

  if (ConsumeCharacterAndWhitespace(input, '*', offset)) {
    if (offset != input.length())
      return;
    syntax_components_.push_back(CSSSyntaxComponent(
        CSSSyntaxType::kTokenStream, g_empty_string, CSSSyntaxRepeat::kNone));
    return;
  }

  do {
    CSSSyntaxType type;
    String ident;
    bool success;

    if (input[offset] == '<') {
      success = ConsumeSyntaxType(input, offset, type);
    } else {
      type = CSSSyntaxType::kIdent;
      success = ConsumeSyntaxIdent(input, offset, ident);
    }

    if (!success) {
      syntax_components_.clear();
      return;
    }

    CSSSyntaxRepeat repeat = CSSSyntaxRepeat::kNone;

    if (ConsumeCharacterAndWhitespace(input, '+', offset))
      repeat = CSSSyntaxRepeat::kSpaceSeparated;
    else if (ConsumeCharacterAndWhitespace(input, '#', offset))
      repeat = CSSSyntaxRepeat::kCommaSeparated;

    // <transform-list> is already a space separated list,
    // <transform-list>+ is invalid.
    // TODO(andruud): Is <transform-list># invalid?
    if (type == CSSSyntaxType::kTransformList &&
        repeat != CSSSyntaxRepeat::kNone) {
      syntax_components_.clear();
      return;
    }
    ConsumeWhitespace(input, offset);
    syntax_components_.push_back(CSSSyntaxComponent(type, ident, repeat));

  } while (ConsumeCharacterAndWhitespace(input, '|', offset));

  if (offset != input.length())
    syntax_components_.clear();
}

const CSSValue* ConsumeSingleType(const CSSSyntaxComponent& syntax,
                                  CSSParserTokenRange& range,
                                  const CSSParserContext* context) {
  using namespace CSSPropertyParserHelpers;

  switch (syntax.type_) {
    case CSSSyntaxType::kIdent:
      if (range.Peek().GetType() == kIdentToken &&
          range.Peek().Value() == syntax.string_) {
        range.ConsumeIncludingWhitespace();
        return CSSCustomIdentValue::Create(AtomicString(syntax.string_));
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
      return ConsumeInteger(range);
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
      return ConsumeCustomIdent(range);
    default:
      NOTREACHED();
      return nullptr;
  }
}

const CSSValue* ConsumeSyntaxComponent(const CSSSyntaxComponent& syntax,
                                       CSSParserTokenRange range,
                                       const CSSParserContext* context) {
  // CSS-wide keywords are already handled by the CSSPropertyParser
  if (syntax.repeat_ == CSSSyntaxRepeat::kSpaceSeparated) {
    CSSValueList* list = CSSValueList::CreateSpaceSeparated();
    while (!range.AtEnd()) {
      const CSSValue* value = ConsumeSingleType(syntax, range, context);
      if (!value)
        return nullptr;
      list->Append(*value);
    }
    return list;
  }
  if (syntax.repeat_ == CSSSyntaxRepeat::kCommaSeparated) {
    CSSValueList* list = CSSValueList::CreateCommaSeparated();
    do {
      const CSSValue* value = ConsumeSingleType(syntax, range, context);
      if (!value)
        return nullptr;
      list->Append(*value);
    } while (CSSPropertyParserHelpers::ConsumeCommaIncludingWhitespace(range));
    return list;
  }
  const CSSValue* result = ConsumeSingleType(syntax, range, context);
  if (!range.AtEnd())
    return nullptr;
  return result;
}

bool CSSSyntaxComponent::CanTake(const CSSStyleValue& value) const {
  switch (type_) {
    case CSSSyntaxType::kTokenStream:
      return value.GetType() == CSSStyleValue::kUnparsedType;
    case CSSSyntaxType::kIdent:
      return value.GetType() == CSSStyleValue::kKeywordType &&
             static_cast<const CSSKeywordValue&>(value).value() == string_;
    case CSSSyntaxType::kLength:
      return CSSOMTypes::IsCSSStyleValueLength(value);
    case CSSSyntaxType::kInteger:
      // TODO(andruud): Support rounding.
      // https://drafts.css-houdini.org/css-typed-om-1/#numeric-objects
      FALLTHROUGH;
    case CSSSyntaxType::kNumber:
      return CSSOMTypes::IsCSSStyleValueNumber(value);
    case CSSSyntaxType::kPercentage:
      return CSSOMTypes::IsCSSStyleValuePercentage(value);
    case CSSSyntaxType::kLengthPercentage:
      // TODO(andruud): Support calc(X% + Ypx).
      return CSSOMTypes::IsCSSStyleValueLength(value) ||
             CSSOMTypes::IsCSSStyleValuePercentage(value);
    case CSSSyntaxType::kColor:
      // TODO(andruud): Support custom properties in CSSUnsupportedStyleValue.
      return false;
    case CSSSyntaxType::kImage:
    case CSSSyntaxType::kUrl:
      return value.GetType() == CSSStyleValue::kURLImageType;
    case CSSSyntaxType::kAngle:
      return CSSOMTypes::IsCSSStyleValueAngle(value);
    case CSSSyntaxType::kTime:
      return CSSOMTypes::IsCSSStyleValueTime(value);
    case CSSSyntaxType::kResolution:
      return CSSOMTypes::IsCSSStyleValueResolution(value);
    case CSSSyntaxType::kTransformFunction:
      // TODO(andruud): Currently not supported by Typed OM.
      // https://github.com/w3c/css-houdini-drafts/issues/290
      // For now, this should accept a CSSUnsupportedStyleValue, such that
      // <transform-function> values can be moved from one registered property
      // to another.
      // TODO(andruud): Support custom properties in CSSUnsupportedStyleValue.
      return false;
    case CSSSyntaxType::kTransformList:
      return value.GetType() == CSSStyleValue::kTransformType;
    case CSSSyntaxType::kCustomIdent:
      return value.GetType() == CSSStyleValue::kKeywordType;
    default:
      return false;
  }
}

bool CSSSyntaxDescriptor::CanTake(const CSSStyleValue& value) const {
  for (const CSSSyntaxComponent& component : syntax_components_) {
    if (component.CanTake(value))
      return true;
  }
  return false;
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

}  // namespace blink

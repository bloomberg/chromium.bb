// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/properties/longhands/content.h"

#include "third_party/blink/renderer/core/css/css_counter_value.h"
#include "third_party/blink/renderer/core/css/css_function_value.h"
#include "third_party/blink/renderer/core/css/css_string_value.h"
#include "third_party/blink/renderer/core/css/css_value_list.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_context.h"
#include "third_party/blink/renderer/core/css/parser/css_property_parser_helpers.h"
#include "third_party/blink/renderer/core/css/properties/computed_style_utils.h"
#include "third_party/blink/renderer/core/css_value_keywords.h"
#include "third_party/blink/renderer/core/style/computed_style.h"

namespace blink {
namespace {

CSSValue* ConsumeAttr(CSSParserTokenRange args,
                      const CSSParserContext& context) {
  if (args.Peek().GetType() != kIdentToken)
    return nullptr;

  AtomicString attr_name =
      args.ConsumeIncludingWhitespace().Value().ToAtomicString();
  if (!args.AtEnd())
    return nullptr;

  if (context.IsHTMLDocument())
    attr_name = attr_name.LowerASCII();

  CSSFunctionValue* attr_value = CSSFunctionValue::Create(CSSValueID::kAttr);
  attr_value->Append(*CSSCustomIdentValue::Create(attr_name));
  return attr_value;
}

CSSValue* ConsumeCounterContent(CSSParserTokenRange args,
                                const CSSParserContext& context,
                                bool counters) {
  CSSCustomIdentValue* identifier =
      css_property_parser_helpers::ConsumeCustomIdent(args, context);
  if (!identifier)
    return nullptr;

  CSSStringValue* separator = nullptr;
  if (!counters) {
    separator = CSSStringValue::Create(String());
  } else {
    if (!css_property_parser_helpers::ConsumeCommaIncludingWhitespace(args) ||
        args.Peek().GetType() != kStringToken)
      return nullptr;
    separator = CSSStringValue::Create(
        args.ConsumeIncludingWhitespace().Value().ToString());
  }

  CSSIdentifierValue* list_style = nullptr;
  if (css_property_parser_helpers::ConsumeCommaIncludingWhitespace(args)) {
    CSSValueID id = args.Peek().Id();
    if ((id != CSSValueID::kNone &&
         (id < CSSValueID::kDisc || id > CSSValueID::kKatakanaIroha)))
      return nullptr;
    list_style = css_property_parser_helpers::ConsumeIdent(args);
  } else {
    list_style = CSSIdentifierValue::Create(CSSValueID::kDecimal);
  }

  if (!args.AtEnd())
    return nullptr;
  return MakeGarbageCollected<cssvalue::CSSCounterValue>(identifier, list_style,
                                                         separator);
}

}  // namespace
namespace css_longhand {

const CSSValue* Content::ParseSingleValue(CSSParserTokenRange& range,
                                          const CSSParserContext& context,
                                          const CSSParserLocalContext&) const {
  if (css_property_parser_helpers::IdentMatches<CSSValueID::kNone,
                                                CSSValueID::kNormal>(
          range.Peek().Id()))
    return css_property_parser_helpers::ConsumeIdent(range);

  CSSValueList* values = CSSValueList::CreateSpaceSeparated();

  do {
    CSSValue* parsed_value =
        css_property_parser_helpers::ConsumeImage(range, &context);
    if (!parsed_value) {
      parsed_value = css_property_parser_helpers::ConsumeIdent<
          CSSValueID::kOpenQuote, CSSValueID::kCloseQuote,
          CSSValueID::kNoOpenQuote, CSSValueID::kNoCloseQuote>(range);
    }
    if (!parsed_value)
      parsed_value = css_property_parser_helpers::ConsumeString(range);
    if (!parsed_value) {
      if (range.Peek().FunctionId() == CSSValueID::kAttr) {
        parsed_value = ConsumeAttr(
            css_property_parser_helpers::ConsumeFunction(range), context);
      } else if (range.Peek().FunctionId() == CSSValueID::kCounter) {
        parsed_value = ConsumeCounterContent(
            css_property_parser_helpers::ConsumeFunction(range), context,
            false);
      } else if (range.Peek().FunctionId() == CSSValueID::kCounters) {
        parsed_value = ConsumeCounterContent(
            css_property_parser_helpers::ConsumeFunction(range), context, true);
      }
      if (!parsed_value)
        return nullptr;
    }
    values->Append(*parsed_value);
  } while (!range.AtEnd());

  return values;
}

const CSSValue* Content::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle&,
    const LayoutObject*,
    Node* styled_node,
    bool allow_visited_style) const {
  return ComputedStyleUtils::ValueForContentData(style);
}

void Content::ApplyInitial(StyleResolverState& state) const {
  state.Style()->SetContent(nullptr);
}

void Content::ApplyInherit(StyleResolverState& state) const {
  // FIXME: In CSS3, it will be possible to inherit content. In CSS2 it is not.
  // This note is a reminder that eventually "inherit" needs to be supported.
}

void Content::ApplyValue(StyleResolverState& state,
                         const CSSValue& value) const {
  if (auto* identifier_value = DynamicTo<CSSIdentifierValue>(value)) {
    DCHECK(identifier_value->GetValueID() == CSSValueID::kNormal ||
           identifier_value->GetValueID() == CSSValueID::kNone);
    state.Style()->SetContent(nullptr);
    return;
  }

  ContentData* first_content = nullptr;
  ContentData* prev_content = nullptr;
  for (auto& item : To<CSSValueList>(value)) {
    ContentData* next_content = nullptr;
    if (item->IsImageGeneratorValue() || item->IsImageSetValue() ||
        item->IsImageValue()) {
      next_content = ContentData::Create(
          state.GetStyleImage(CSSPropertyID::kContent, *item));
    } else if (const auto* counter_value =
                   DynamicTo<cssvalue::CSSCounterValue>(item.Get())) {
      const auto list_style_type =
          CssValueIDToPlatformEnum<EListStyleType>(counter_value->ListStyle());
      std::unique_ptr<CounterContent> counter =
          std::make_unique<CounterContent>(
              AtomicString(counter_value->Identifier()), list_style_type,
              AtomicString(counter_value->Separator()));
      next_content = ContentData::Create(std::move(counter));
    } else if (auto* item_identifier_value =
                   DynamicTo<CSSIdentifierValue>(item.Get())) {
      QuoteType quote_type;
      switch (item_identifier_value->GetValueID()) {
        default:
          NOTREACHED();
          FALLTHROUGH;
        case CSSValueID::kOpenQuote:
          quote_type = QuoteType::kOpen;
          break;
        case CSSValueID::kCloseQuote:
          quote_type = QuoteType::kClose;
          break;
        case CSSValueID::kNoOpenQuote:
          quote_type = QuoteType::kNoOpen;
          break;
        case CSSValueID::kNoCloseQuote:
          quote_type = QuoteType::kNoClose;
          break;
      }
      next_content = ContentData::Create(quote_type);
    } else {
      String string;
      if (const auto* function_value =
              DynamicTo<CSSFunctionValue>(item.Get())) {
        DCHECK_EQ(function_value->FunctionType(), CSSValueID::kAttr);
        state.Style()->SetHasAttrContent();
        // TODO: Can a namespace be specified for an attr(foo)?
        QualifiedName attr(
            g_null_atom,
            To<CSSCustomIdentValue>(function_value->Item(0)).Value(),
            g_null_atom);
        const AtomicString& attr_value = state.GetElement()->getAttribute(attr);
        string = attr_value.IsNull() ? g_empty_string : attr_value.GetString();
      } else {
        string = To<CSSStringValue>(*item).Value();
      }
      if (prev_content && prev_content->IsText()) {
        TextContentData* text_content = To<TextContentData>(prev_content);
        text_content->SetText(text_content->GetText() + string);
        continue;
      }
      next_content = ContentData::Create(string);
    }

    if (!first_content)
      first_content = next_content;
    else
      prev_content->SetNext(next_content);

    prev_content = next_content;
  }
  DCHECK(first_content);
  state.Style()->SetContent(first_content);
}

}  // namespace css_longhand
}  // namespace blink

// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/properties/CSSPropertyAPIContent.h"

#include "core/CSSValueKeywords.h"
#include "core/css/CSSCounterValue.h"
#include "core/css/CSSFunctionValue.h"
#include "core/css/CSSStringValue.h"
#include "core/css/CSSValueList.h"
#include "core/css/parser/CSSParserContext.h"
#include "core/css/parser/CSSPropertyParserHelpers.h"

class CSSParserLocalContext;
namespace blink {

namespace {

CSSValue* ConsumeAttr(CSSParserTokenRange args,
                      const CSSParserContext* context) {
  if (args.Peek().GetType() != kIdentToken)
    return nullptr;

  AtomicString attr_name =
      args.ConsumeIncludingWhitespace().Value().ToAtomicString();
  if (!args.AtEnd())
    return nullptr;

  if (context->IsHTMLDocument())
    attr_name = attr_name.LowerASCII();

  CSSFunctionValue* attr_value = CSSFunctionValue::Create(CSSValueAttr);
  attr_value->Append(*CSSCustomIdentValue::Create(attr_name));
  return attr_value;
}

CSSValue* ConsumeCounterContent(CSSParserTokenRange args, bool counters) {
  CSSCustomIdentValue* identifier =
      CSSPropertyParserHelpers::ConsumeCustomIdent(args);
  if (!identifier)
    return nullptr;

  CSSStringValue* separator = nullptr;
  if (!counters) {
    separator = CSSStringValue::Create(String());
  } else {
    if (!CSSPropertyParserHelpers::ConsumeCommaIncludingWhitespace(args) ||
        args.Peek().GetType() != kStringToken)
      return nullptr;
    separator = CSSStringValue::Create(
        args.ConsumeIncludingWhitespace().Value().ToString());
  }

  CSSIdentifierValue* list_style = nullptr;
  if (CSSPropertyParserHelpers::ConsumeCommaIncludingWhitespace(args)) {
    CSSValueID id = args.Peek().Id();
    if ((id != CSSValueNone &&
         (id < CSSValueDisc || id > CSSValueKatakanaIroha)))
      return nullptr;
    list_style = CSSPropertyParserHelpers::ConsumeIdent(args);
  } else {
    list_style = CSSIdentifierValue::Create(CSSValueDecimal);
  }

  if (!args.AtEnd())
    return nullptr;
  return cssvalue::CSSCounterValue::Create(identifier, list_style, separator);
}

}  // namespace

const CSSValue* CSSPropertyAPIContent::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) {
  if (CSSPropertyParserHelpers::IdentMatches<CSSValueNone, CSSValueNormal>(
          range.Peek().Id()))
    return CSSPropertyParserHelpers::ConsumeIdent(range);

  CSSValueList* values = CSSValueList::CreateSpaceSeparated();

  do {
    CSSValue* parsed_value =
        CSSPropertyParserHelpers::ConsumeImage(range, &context);
    if (!parsed_value) {
      parsed_value = CSSPropertyParserHelpers::ConsumeIdent<
          CSSValueOpenQuote, CSSValueCloseQuote, CSSValueNoOpenQuote,
          CSSValueNoCloseQuote>(range);
    }
    if (!parsed_value)
      parsed_value = CSSPropertyParserHelpers::ConsumeString(range);
    if (!parsed_value) {
      if (range.Peek().FunctionId() == CSSValueAttr) {
        parsed_value = ConsumeAttr(
            CSSPropertyParserHelpers::ConsumeFunction(range), &context);
      } else if (range.Peek().FunctionId() == CSSValueCounter) {
        parsed_value = ConsumeCounterContent(
            CSSPropertyParserHelpers::ConsumeFunction(range), false);
      } else if (range.Peek().FunctionId() == CSSValueCounters) {
        parsed_value = ConsumeCounterContent(
            CSSPropertyParserHelpers::ConsumeFunction(range), true);
      }
      if (!parsed_value)
        return nullptr;
    }
    values->Append(*parsed_value);
  } while (!range.AtEnd());

  return values;
}

}  // namespace blink

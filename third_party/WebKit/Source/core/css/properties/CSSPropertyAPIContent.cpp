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

namespace blink {

namespace {

CSSValue* consumeAttr(CSSParserTokenRange args,
                      const CSSParserContext* context) {
  if (args.peek().type() != IdentToken)
    return nullptr;

  AtomicString attrName =
      args.consumeIncludingWhitespace().value().toAtomicString();
  if (!args.atEnd())
    return nullptr;

  // TODO(esprehn): This should be lowerASCII().
  if (context->isHTMLDocument())
    attrName = attrName.lower();

  CSSFunctionValue* attrValue = CSSFunctionValue::create(CSSValueAttr);
  attrValue->append(*CSSCustomIdentValue::create(attrName));
  return attrValue;
}

CSSValue* consumeCounterContent(CSSParserTokenRange args, bool counters) {
  CSSCustomIdentValue* identifier =
      CSSPropertyParserHelpers::consumeCustomIdent(args);
  if (!identifier)
    return nullptr;

  CSSStringValue* separator = nullptr;
  if (!counters) {
    separator = CSSStringValue::create(String());
  } else {
    if (!CSSPropertyParserHelpers::consumeCommaIncludingWhitespace(args) ||
        args.peek().type() != StringToken)
      return nullptr;
    separator = CSSStringValue::create(
        args.consumeIncludingWhitespace().value().toString());
  }

  CSSIdentifierValue* listStyle = nullptr;
  if (CSSPropertyParserHelpers::consumeCommaIncludingWhitespace(args)) {
    CSSValueID id = args.peek().id();
    if ((id != CSSValueNone &&
         (id < CSSValueDisc || id > CSSValueKatakanaIroha)))
      return nullptr;
    listStyle = CSSPropertyParserHelpers::consumeIdent(args);
  } else {
    listStyle = CSSIdentifierValue::create(CSSValueDecimal);
  }

  if (!args.atEnd())
    return nullptr;
  return CSSCounterValue::create(identifier, listStyle, separator);
}

}  // namespace

const CSSValue* CSSPropertyAPIContent::parseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext* context) {
  if (CSSPropertyParserHelpers::identMatches<CSSValueNone, CSSValueNormal>(
          range.peek().id()))
    return CSSPropertyParserHelpers::consumeIdent(range);

  CSSValueList* values = CSSValueList::createSpaceSeparated();

  do {
    CSSValue* parsedValue =
        CSSPropertyParserHelpers::consumeImage(range, context);
    if (!parsedValue) {
      parsedValue = CSSPropertyParserHelpers::consumeIdent<
          CSSValueOpenQuote, CSSValueCloseQuote, CSSValueNoOpenQuote,
          CSSValueNoCloseQuote>(range);
    }
    if (!parsedValue)
      parsedValue = CSSPropertyParserHelpers::consumeString(range);
    if (!parsedValue) {
      if (range.peek().functionId() == CSSValueAttr) {
        parsedValue = consumeAttr(
            CSSPropertyParserHelpers::consumeFunction(range), context);
      } else if (range.peek().functionId() == CSSValueCounter) {
        parsedValue = consumeCounterContent(
            CSSPropertyParserHelpers::consumeFunction(range), false);
      } else if (range.peek().functionId() == CSSValueCounters) {
        parsedValue = consumeCounterContent(
            CSSPropertyParserHelpers::consumeFunction(range), true);
      }
      if (!parsedValue)
        return nullptr;
    }
    values->append(*parsedValue);
  } while (!range.atEnd());

  return values;
}

}  // namespace blink

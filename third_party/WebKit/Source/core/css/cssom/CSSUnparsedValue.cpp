// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/cssom/CSSUnparsedValue.h"

#include "core/css/CSSVariableData.h"
#include "core/css/CSSVariableReferenceValue.h"
#include "core/css/cssom/CSSStyleVariableReferenceValue.h"
#include "core/css/parser/CSSTokenizer.h"
#include "platform/wtf/text/StringBuilder.h"

namespace blink {

namespace {

StringView FindVariableName(CSSParserTokenRange& range) {
  range.ConsumeWhitespace();
  return range.Consume().Value();
}

StringOrCSSVariableReferenceValue VariableReferenceValue(
    const StringView& variable_name,
    const HeapVector<StringOrCSSVariableReferenceValue>& tokens) {
  CSSUnparsedValue* unparsed_value;
  if (tokens.size() == 0)
    unparsed_value = nullptr;
  else
    unparsed_value = CSSUnparsedValue::Create(tokens);

  CSSStyleVariableReferenceValue* variable_reference =
      CSSStyleVariableReferenceValue::Create(variable_name.ToString(),
                                             unparsed_value);
  return StringOrCSSVariableReferenceValue::FromCSSVariableReferenceValue(
      variable_reference);
}

HeapVector<StringOrCSSVariableReferenceValue> ParserTokenRangeToTokens(
    CSSParserTokenRange range) {
  HeapVector<StringOrCSSVariableReferenceValue> tokens;
  StringBuilder builder;
  while (!range.AtEnd()) {
    if (range.Peek().FunctionId() == CSSValueVar) {
      if (!builder.IsEmpty()) {
        tokens.push_back(
            StringOrCSSVariableReferenceValue::FromString(builder.ToString()));
        builder.Clear();
      }
      CSSParserTokenRange block = range.ConsumeBlock();
      StringView variable_name = FindVariableName(block);
      block.ConsumeWhitespace();
      if (block.Peek().GetType() == CSSParserTokenType::kCommaToken)
        block.Consume();
      tokens.push_back(VariableReferenceValue(variable_name,
                                              ParserTokenRangeToTokens(block)));
    } else {
      range.Consume().Serialize(builder);
    }
  }
  if (!builder.IsEmpty()) {
    tokens.push_back(
        StringOrCSSVariableReferenceValue::FromString(builder.ToString()));
  }
  return tokens;
}

}  // namespace

CSSUnparsedValue* CSSUnparsedValue::FromCSSValue(
    const CSSVariableReferenceValue& value) {
  DCHECK(value.VariableDataValue());
  return FromCSSValue(*value.VariableDataValue());
}

CSSUnparsedValue* CSSUnparsedValue::FromCSSValue(const CSSVariableData& value) {
  return CSSUnparsedValue::Create(ParserTokenRangeToTokens(value.TokenRange()));
}

const CSSValue* CSSUnparsedValue::ToCSSValue() const {
  CSSTokenizer tokenizer(ToString());
  const auto tokens = tokenizer.TokenizeToEOF();
  return CSSVariableReferenceValue::Create(CSSVariableData::Create(
      CSSParserTokenRange(tokens), false /* isAnimationTainted */,
      false /* needsVariableResolution */));
}

String CSSUnparsedValue::ToString() const {
  StringBuilder input;

  for (unsigned i = 0; i < tokens_.size(); i++) {
    if (i) {
      input.Append("/**/");
    }
    if (tokens_[i].IsString()) {
      input.Append(tokens_[i].GetAsString());
    } else if (tokens_[i].IsCSSVariableReferenceValue()) {
      const auto* reference_value = tokens_[i].GetAsCSSVariableReferenceValue();
      input.Append("var(");
      input.Append(reference_value->variable());
      if (reference_value->fallback()) {
        input.Append(",");
        input.Append(reference_value->fallback()->ToString());
      }
      input.Append(")");
    } else {
      NOTREACHED();
    }
  }

  return input.ToString();
}

}  // namespace blink

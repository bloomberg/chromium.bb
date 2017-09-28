// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/cssom/CSSUnparsedValue.h"

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
    const HeapVector<StringOrCSSVariableReferenceValue>& fragments) {
  CSSUnparsedValue* unparsed_value;
  if (fragments.size() == 0)
    unparsed_value = nullptr;
  else
    unparsed_value = CSSUnparsedValue::Create(fragments);

  CSSStyleVariableReferenceValue* variable_reference =
      CSSStyleVariableReferenceValue::Create(variable_name.ToString(),
                                             unparsed_value);
  return StringOrCSSVariableReferenceValue::FromCSSVariableReferenceValue(
      variable_reference);
}

HeapVector<StringOrCSSVariableReferenceValue> ParserTokenRangeToFragments(
    CSSParserTokenRange range) {
  HeapVector<StringOrCSSVariableReferenceValue> fragments;
  StringBuilder builder;
  while (!range.AtEnd()) {
    if (range.Peek().FunctionId() == CSSValueVar) {
      if (!builder.IsEmpty()) {
        fragments.push_back(
            StringOrCSSVariableReferenceValue::FromString(builder.ToString()));
        builder.Clear();
      }
      CSSParserTokenRange block = range.ConsumeBlock();
      StringView variable_name = FindVariableName(block);
      block.ConsumeWhitespace();
      if (block.Peek().GetType() == CSSParserTokenType::kCommaToken)
        block.Consume();
      fragments.push_back(VariableReferenceValue(
          variable_name, ParserTokenRangeToFragments(block)));
    } else {
      range.Consume().Serialize(builder);
    }
  }
  if (!builder.IsEmpty()) {
    fragments.push_back(
        StringOrCSSVariableReferenceValue::FromString(builder.ToString()));
  }
  return fragments;
}

}  // namespace

CSSUnparsedValue* CSSUnparsedValue::FromCSSValue(
    const CSSVariableReferenceValue& css_variable_reference_value) {
  return CSSUnparsedValue::Create(ParserTokenRangeToFragments(
      css_variable_reference_value.VariableDataValue()->TokenRange()));
}

const CSSValue* CSSUnparsedValue::ToCSSValue() const {
  StringBuilder input;

  for (unsigned i = 0; i < fragments_.size(); i++) {
    if (i) {
      input.Append("/**/");
    }
    if (fragments_[i].IsString()) {
      input.Append(fragments_[i].GetAsString());
    } else if (fragments_[i].IsCSSVariableReferenceValue()) {
      input.Append(fragments_[i].GetAsCSSVariableReferenceValue()->variable());
    } else {
      NOTREACHED();
    }
  }

  CSSTokenizer tokenizer(input.ToString());
  const auto tokens = tokenizer.TokenizeToEOF();
  // TODO(alancutter): This should be using a real parser context instead of
  // StrictCSSParserContext.
  return CSSVariableReferenceValue::Create(
      CSSVariableData::Create(CSSParserTokenRange(tokens),
                              false /* isAnimationTainted */,
                              true /* needsVariableResolution */),
      *StrictCSSParserContext());
}

}  // namespace blink

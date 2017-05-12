// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/parser/CSSVariableParser.h"

#include "core/css/CSSCustomPropertyDeclaration.h"
#include "core/css/CSSVariableReferenceValue.h"
#include "core/css/parser/CSSParserTokenRange.h"

namespace blink {

bool CSSVariableParser::IsValidVariableName(const CSSParserToken& token) {
  if (token.GetType() != kIdentToken)
    return false;

  StringView value = token.Value();
  return value.length() >= 2 && value[0] == '-' && value[1] == '-';
}

bool CSSVariableParser::IsValidVariableName(const String& string) {
  return string.length() >= 2 && string[0] == '-' && string[1] == '-';
}

bool IsValidVariableReference(CSSParserTokenRange, bool& has_at_apply_rule);

bool ClassifyBlock(CSSParserTokenRange range,
                   bool& has_references,
                   bool& has_at_apply_rule,
                   bool is_top_level_block = true) {
  while (!range.AtEnd()) {
    if (range.Peek().GetBlockType() == CSSParserToken::kBlockStart) {
      const CSSParserToken& token = range.Peek();
      CSSParserTokenRange block = range.ConsumeBlock();
      if (token.FunctionId() == CSSValueVar) {
        if (!IsValidVariableReference(block, has_at_apply_rule))
          return false;  // Bail if any references are invalid
        has_references = true;
        continue;
      }
      if (!ClassifyBlock(block, has_references, has_at_apply_rule, false))
        return false;
      continue;
    }

    DCHECK_NE(range.Peek().GetBlockType(), CSSParserToken::kBlockEnd);

    const CSSParserToken& token = range.Consume();
    switch (token.GetType()) {
      case kAtKeywordToken: {
        if (EqualIgnoringASCIICase(token.Value(), "apply")) {
          range.ConsumeWhitespace();
          const CSSParserToken& variable_name =
              range.ConsumeIncludingWhitespace();
          if (!CSSVariableParser::IsValidVariableName(variable_name) ||
              !(range.AtEnd() || range.Peek().GetType() == kSemicolonToken ||
                range.Peek().GetType() == kRightBraceToken))
            return false;
          has_at_apply_rule = true;
        }
        break;
      }
      case kDelimiterToken: {
        if (token.Delimiter() == '!' && is_top_level_block)
          return false;
        break;
      }
      case kRightParenthesisToken:
      case kRightBraceToken:
      case kRightBracketToken:
      case kBadStringToken:
      case kBadUrlToken:
        return false;
      case kSemicolonToken:
        if (is_top_level_block)
          return false;
        break;
      default:
        break;
    }
  }
  return true;
}

bool IsValidVariableReference(CSSParserTokenRange range,
                              bool& has_at_apply_rule) {
  range.ConsumeWhitespace();
  if (!CSSVariableParser::IsValidVariableName(
          range.ConsumeIncludingWhitespace()))
    return false;
  if (range.AtEnd())
    return true;

  if (range.Consume().GetType() != kCommaToken)
    return false;
  if (range.AtEnd())
    return false;

  bool has_references = false;
  return ClassifyBlock(range, has_references, has_at_apply_rule);
}

static CSSValueID ClassifyVariableRange(CSSParserTokenRange range,
                                        bool& has_references,
                                        bool& has_at_apply_rule) {
  has_references = false;
  has_at_apply_rule = false;

  range.ConsumeWhitespace();
  if (range.Peek().GetType() == kIdentToken) {
    CSSValueID id = range.ConsumeIncludingWhitespace().Id();
    if (range.AtEnd() &&
        (id == CSSValueInherit || id == CSSValueInitial || id == CSSValueUnset))
      return id;
  }

  if (ClassifyBlock(range, has_references, has_at_apply_rule))
    return CSSValueInternalVariableValue;
  return CSSValueInvalid;
}

bool CSSVariableParser::ContainsValidVariableReferences(
    CSSParserTokenRange range) {
  bool has_references;
  bool has_at_apply_rule;
  CSSValueID type =
      ClassifyVariableRange(range, has_references, has_at_apply_rule);
  return type == CSSValueInternalVariableValue && has_references &&
         !has_at_apply_rule;
}

CSSCustomPropertyDeclaration* CSSVariableParser::ParseDeclarationValue(
    const AtomicString& variable_name,
    CSSParserTokenRange range,
    bool is_animation_tainted) {
  if (range.AtEnd())
    return nullptr;

  bool has_references;
  bool has_at_apply_rule;
  CSSValueID type =
      ClassifyVariableRange(range, has_references, has_at_apply_rule);

  if (type == CSSValueInvalid)
    return nullptr;
  if (type == CSSValueInternalVariableValue) {
    return CSSCustomPropertyDeclaration::Create(
        variable_name,
        CSSVariableData::Create(range, is_animation_tainted,
                                has_references || has_at_apply_rule));
  }
  return CSSCustomPropertyDeclaration::Create(variable_name, type);
}

CSSVariableReferenceValue* CSSVariableParser::ParseRegisteredPropertyValue(
    CSSParserTokenRange range,
    const CSSParserContext& context,
    bool require_var_reference,
    bool is_animation_tainted) {
  if (range.AtEnd())
    return nullptr;

  bool has_references;
  bool has_at_apply_rule;
  CSSValueID type =
      ClassifyVariableRange(range, has_references, has_at_apply_rule);

  if (type != CSSValueInternalVariableValue)
    return nullptr;  // Invalid or a css-wide keyword
  if (require_var_reference && !has_references)
    return nullptr;
  // TODO(timloh): Should this be hasReferences || hasAtApplyRule?
  return CSSVariableReferenceValue::Create(
      CSSVariableData::Create(range, is_animation_tainted, has_references),
      context);
}

}  // namespace blink

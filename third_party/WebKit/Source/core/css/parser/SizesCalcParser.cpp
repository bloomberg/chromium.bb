// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/parser/SizesCalcParser.h"

#include "core/css/MediaValues.h"
#include "core/css/parser/CSSParserToken.h"

namespace blink {

SizesCalcParser::SizesCalcParser(CSSParserTokenRange range,
                                 MediaValues* mediaValues)
    : m_mediaValues(mediaValues), m_result(0) {
  m_isValid = calcToReversePolishNotation(range) && calculate();
}

float SizesCalcParser::result() const {
  ASSERT(m_isValid);
  return m_result;
}

static bool operatorPriority(UChar cc, bool& highPriority) {
  if (cc == '+' || cc == '-')
    highPriority = false;
  else if (cc == '*' || cc == '/')
    highPriority = true;
  else
    return false;
  return true;
}

bool SizesCalcParser::handleOperator(Vector<CSSParserToken>& stack,
                                     const CSSParserToken& token) {
  // If the token is an operator, o1, then:
  // while there is an operator token, o2, at the top of the stack, and
  // either o1 is left-associative and its precedence is equal to that of o2,
  // or o1 has precedence less than that of o2,
  // pop o2 off the stack, onto the output queue;
  // push o1 onto the stack.
  bool stackOperatorPriority;
  bool incomingOperatorPriority;

  if (!operatorPriority(token.delimiter(), incomingOperatorPriority))
    return false;
  if (!stack.isEmpty() && stack.back().type() == DelimiterToken) {
    if (!operatorPriority(stack.back().delimiter(), stackOperatorPriority))
      return false;
    if (!incomingOperatorPriority || stackOperatorPriority) {
      appendOperator(stack.back());
      stack.pop_back();
    }
  }
  stack.push_back(token);
  return true;
}

void SizesCalcParser::appendNumber(const CSSParserToken& token) {
  SizesCalcValue value;
  value.value = token.numericValue();
  m_valueList.push_back(value);
}

bool SizesCalcParser::appendLength(const CSSParserToken& token) {
  SizesCalcValue value;
  double result = 0;
  if (!m_mediaValues->computeLength(token.numericValue(), token.unitType(),
                                    result))
    return false;
  value.value = result;
  value.isLength = true;
  m_valueList.push_back(value);
  return true;
}

void SizesCalcParser::appendOperator(const CSSParserToken& token) {
  SizesCalcValue value;
  value.operation = token.delimiter();
  m_valueList.push_back(value);
}

bool SizesCalcParser::calcToReversePolishNotation(CSSParserTokenRange range) {
  // This method implements the shunting yard algorithm, to turn the calc syntax
  // into a reverse polish notation.
  // http://en.wikipedia.org/wiki/Shunting-yard_algorithm

  Vector<CSSParserToken> stack;
  while (!range.atEnd()) {
    const CSSParserToken& token = range.consume();
    switch (token.type()) {
      case NumberToken:
        appendNumber(token);
        break;
      case DimensionToken:
        if (!CSSPrimitiveValue::isLength(token.unitType()) ||
            !appendLength(token))
          return false;
        break;
      case DelimiterToken:
        if (!handleOperator(stack, token))
          return false;
        break;
      case FunctionToken:
        if (!equalIgnoringASCIICase(token.value(), "calc"))
          return false;
      // "calc(" is the same as "("
      case LeftParenthesisToken:
        // If the token is a left parenthesis, then push it onto the stack.
        stack.push_back(token);
        break;
      case RightParenthesisToken:
        // If the token is a right parenthesis:
        // Until the token at the top of the stack is a left parenthesis, pop
        // operators off the stack onto the output queue.
        while (!stack.isEmpty() &&
               stack.back().type() != LeftParenthesisToken &&
               stack.back().type() != FunctionToken) {
          appendOperator(stack.back());
          stack.pop_back();
        }
        // If the stack runs out without finding a left parenthesis, then there
        // are mismatched parentheses.
        if (stack.isEmpty())
          return false;
        // Pop the left parenthesis from the stack, but not onto the output
        // queue.
        stack.pop_back();
        break;
      case WhitespaceToken:
      case EOFToken:
        break;
      case CommentToken:
        ASSERT_NOT_REACHED();
      case CDOToken:
      case CDCToken:
      case AtKeywordToken:
      case HashToken:
      case UrlToken:
      case BadUrlToken:
      case PercentageToken:
      case IncludeMatchToken:
      case DashMatchToken:
      case PrefixMatchToken:
      case SuffixMatchToken:
      case SubstringMatchToken:
      case ColumnToken:
      case UnicodeRangeToken:
      case IdentToken:
      case CommaToken:
      case ColonToken:
      case SemicolonToken:
      case LeftBraceToken:
      case LeftBracketToken:
      case RightBraceToken:
      case RightBracketToken:
      case StringToken:
      case BadStringToken:
        return false;
    }
  }

  // When there are no more tokens to read:
  // While there are still operator tokens in the stack:
  while (!stack.isEmpty()) {
    // If the operator token on the top of the stack is a parenthesis, then
    // there are mismatched parentheses.
    CSSParserTokenType type = stack.back().type();
    if (type == LeftParenthesisToken || type == FunctionToken)
      return false;
    // Pop the operator onto the output queue.
    appendOperator(stack.back());
    stack.pop_back();
  }
  return true;
}

static bool operateOnStack(Vector<SizesCalcValue>& stack, UChar operation) {
  if (stack.size() < 2)
    return false;
  SizesCalcValue rightOperand = stack.back();
  stack.pop_back();
  SizesCalcValue leftOperand = stack.back();
  stack.pop_back();
  bool isLength;
  switch (operation) {
    case '+':
      if (rightOperand.isLength != leftOperand.isLength)
        return false;
      isLength = (rightOperand.isLength && leftOperand.isLength);
      stack.push_back(
          SizesCalcValue(leftOperand.value + rightOperand.value, isLength));
      break;
    case '-':
      if (rightOperand.isLength != leftOperand.isLength)
        return false;
      isLength = (rightOperand.isLength && leftOperand.isLength);
      stack.push_back(
          SizesCalcValue(leftOperand.value - rightOperand.value, isLength));
      break;
    case '*':
      if (rightOperand.isLength && leftOperand.isLength)
        return false;
      isLength = (rightOperand.isLength || leftOperand.isLength);
      stack.push_back(
          SizesCalcValue(leftOperand.value * rightOperand.value, isLength));
      break;
    case '/':
      if (rightOperand.isLength || rightOperand.value == 0)
        return false;
      stack.push_back(SizesCalcValue(leftOperand.value / rightOperand.value,
                                     leftOperand.isLength));
      break;
    default:
      return false;
  }
  return true;
}

bool SizesCalcParser::calculate() {
  Vector<SizesCalcValue> stack;
  for (const auto& value : m_valueList) {
    if (value.operation == 0) {
      stack.push_back(value);
    } else {
      if (!operateOnStack(stack, value.operation))
        return false;
    }
  }
  if (stack.size() == 1 && stack.back().isLength) {
    m_result = std::max(clampTo<float>(stack.back().value), (float)0.0);
    return true;
  }
  return false;
}

}  // namespace blink

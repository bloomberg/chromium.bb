// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/cssom/CSSNumericValue.h"

#include "bindings/core/v8/ExceptionState.h"
#include "core/css/CSSCalculationValue.h"
#include "core/css/CSSPrimitiveValue.h"
#include "core/css/cssom/CSSMathInvert.h"
#include "core/css/cssom/CSSMathMax.h"
#include "core/css/cssom/CSSMathMin.h"
#include "core/css/cssom/CSSMathNegate.h"
#include "core/css/cssom/CSSMathProduct.h"
#include "core/css/cssom/CSSMathSum.h"
#include "core/css/cssom/CSSUnitValue.h"
#include "core/css/parser/CSSParserTokenStream.h"
#include "core/css/parser/CSSTokenizer.h"

namespace blink {

namespace {

template <CSSStyleValue::StyleValueType type>
void PrependValueForArithmetic(CSSNumericValueVector& vector,
                               CSSNumericValue* value) {
  DCHECK(value);
  if (value->GetType() == type)
    vector.PrependVector(static_cast<CSSMathVariadic*>(value)->NumericValues());
  else
    vector.push_front(value);
}

template <class BinaryOperation>
CSSUnitValue* MaybeSimplifyAsUnitValue(const CSSNumericValueVector& values,
                                       const BinaryOperation& op) {
  DCHECK(!values.IsEmpty());

  CSSUnitValue* first_unit_value = ToCSSUnitValueOrNull(values[0]);
  if (!first_unit_value)
    return nullptr;

  double final_value = first_unit_value->value();
  for (size_t i = 1; i < values.size(); i++) {
    CSSUnitValue* unit_value = ToCSSUnitValueOrNull(values[i]);
    if (!unit_value ||
        unit_value->GetInternalUnit() != first_unit_value->GetInternalUnit())
      return nullptr;

    final_value = op(final_value, unit_value->value());
  }

  return CSSUnitValue::Create(final_value, first_unit_value->GetInternalUnit());
}

CalcOperator CanonicalOperator(CalcOperator op) {
  if (op == kCalcAdd || op == kCalcSubtract)
    return kCalcAdd;
  return kCalcMultiply;
}

bool CanCombineNodes(const CSSCalcExpressionNode& root,
                     const CSSCalcExpressionNode& node) {
  DCHECK_EQ(root.GetType(), CSSCalcExpressionNode::kCssCalcBinaryOperation);
  if (node.GetType() == CSSCalcExpressionNode::kCssCalcPrimitiveValue)
    return false;
  return !node.IsNestedCalc() && CanonicalOperator(root.OperatorType()) ==
                                     CanonicalOperator(node.OperatorType());
}

CSSNumericValue* NegateOrInvertIfRequired(CalcOperator parent_op,
                                          CSSNumericValue* value) {
  DCHECK(value);
  if (parent_op == kCalcSubtract)
    return CSSMathNegate::Create(value);
  if (parent_op == kCalcDivide)
    return CSSMathInvert::Create(value);
  return value;
}

CSSNumericValue* CalcToNumericValue(const CSSCalcExpressionNode& root) {
  if (root.GetType() == CSSCalcExpressionNode::kCssCalcPrimitiveValue) {
    const CSSPrimitiveValue::UnitType unit = root.TypeWithCalcResolved();
    auto* value = CSSUnitValue::Create(
        root.DoubleValue(), unit == CSSPrimitiveValue::UnitType::kInteger
                                ? CSSPrimitiveValue::UnitType::kNumber
                                : unit);
    DCHECK(value);

    // For cases like calc(1), we need to wrap the value in a CSSMathSum
    if (!root.IsNestedCalc())
      return value;

    CSSNumericValueVector values;
    values.push_back(value);
    return CSSMathSum::Create(std::move(values));
  }

  // When the node is a binary operator, we return either a CSSMathSum or a
  // CSSMathProduct.
  DCHECK_EQ(root.GetType(), CSSCalcExpressionNode::kCssCalcBinaryOperation);
  CSSNumericValueVector values;

  // For cases like calc(1 + 2 + 3), the calc expression tree looks like:
  //       +
  //      / \
  //     +   3
  //    / \
  //   1   2
  //
  // But we want to produce a CSSMathValue tree that looks like:
  //       +
  //      /|\
  //     1 2 3
  //
  // So when the left child has the same operator as its parent, we can combine
  // the two nodes. We keep moving down the left side of the tree as long as the
  // current node and the root can be combined, collecting the right child of
  // the nodes that we encounter.
  const CSSCalcExpressionNode* cur_node = &root;
  do {
    DCHECK(cur_node->LeftExpressionNode());
    DCHECK(cur_node->RightExpressionNode());

    const auto& value = CalcToNumericValue(*cur_node->RightExpressionNode());

    // If the current node is a '-' or '/', it's really just a '+' or '*' with
    // the right child negated or inverted, respectively.
    values.push_back(NegateOrInvertIfRequired(cur_node->OperatorType(), value));
    cur_node = cur_node->LeftExpressionNode();
  } while (CanCombineNodes(root, *cur_node));

  DCHECK(cur_node);
  values.push_back(CalcToNumericValue(*cur_node));

  // Our algorithm collects the children in reverse order, so we have to reverse
  // the values.
  std::reverse(values.begin(), values.end());
  if (root.OperatorType() == kCalcAdd || root.OperatorType() == kCalcSubtract)
    return CSSMathSum::Create(std::move(values));
  return CSSMathProduct::Create(std::move(values));
}

}  // namespace

bool CSSNumericValue::IsValidUnit(CSSPrimitiveValue::UnitType unit) {
  // UserUnits returns true for CSSPrimitiveValue::IsLength below.
  if (unit == CSSPrimitiveValue::UnitType::kUserUnits)
    return false;
  if (unit == CSSPrimitiveValue::UnitType::kNumber ||
      unit == CSSPrimitiveValue::UnitType::kPercentage ||
      CSSPrimitiveValue::IsLength(unit) || CSSPrimitiveValue::IsAngle(unit) ||
      CSSPrimitiveValue::IsTime(unit) || CSSPrimitiveValue::IsFrequency(unit) ||
      CSSPrimitiveValue::IsResolution(unit) || CSSPrimitiveValue::IsFlex(unit))
    return true;
  return false;
}

CSSPrimitiveValue::UnitType CSSNumericValue::UnitFromName(const String& name) {
  if (EqualIgnoringASCIICase(name, "number"))
    return CSSPrimitiveValue::UnitType::kNumber;
  if (EqualIgnoringASCIICase(name, "percent") || name == "%")
    return CSSPrimitiveValue::UnitType::kPercentage;
  return CSSPrimitiveValue::StringToUnitType(name);
}

CSSNumericValue* CSSNumericValue::parse(const String& css_text,
                                        ExceptionState& exception_state) {
  CSSTokenizer tokenizer(css_text);
  CSSParserTokenStream stream(tokenizer);
  auto range = stream.ConsumeUntilPeekedTypeIs<>();
  if (!stream.AtEnd()) {
    exception_state.ThrowDOMException(kSyntaxError, "Invalid math expression");
    return nullptr;
  }

  switch (range.Peek().GetType()) {
    case kNumberToken:
    case kPercentageToken:
    case kDimensionToken: {
      const auto token = range.Consume();
      if (!range.AtEnd())
        break;
      return CSSUnitValue::Create(token.NumericValue(), token.GetUnitType());
    }
    case kFunctionToken:
      if (range.Peek().FunctionId() == CSSValueCalc ||
          range.Peek().FunctionId() == CSSValueWebkitCalc) {
        CSSCalcValue* calc_value = CSSCalcValue::Create(range, kValueRangeAll);
        if (!calc_value)
          break;

        DCHECK(calc_value->ExpressionNode());
        return CalcToNumericValue(*calc_value->ExpressionNode());
      }
    default:
      break;
  }

  exception_state.ThrowDOMException(kSyntaxError, "Invalid math expression");
  return nullptr;
}

CSSNumericValue* CSSNumericValue::FromCSSValue(const CSSPrimitiveValue& value) {
  if (value.IsCalculated()) {
    // TODO(meade): Implement this case.
    return nullptr;
  }
  return CSSUnitValue::FromCSSValue(value);
}

/* static */
CSSNumericValue* CSSNumericValue::FromNumberish(const CSSNumberish& value) {
  if (value.IsDouble()) {
    return CSSUnitValue::Create(value.GetAsDouble(),
                                CSSPrimitiveValue::UnitType::kNumber);
  }
  return value.GetAsCSSNumericValue();
}

CSSNumericValue* CSSNumericValue::to(const String& unit_string,
                                     ExceptionState& exception_state) {
  CSSPrimitiveValue::UnitType target_unit = UnitFromName(unit_string);
  if (!IsValidUnit(target_unit)) {
    exception_state.ThrowDOMException(kSyntaxError,
                                      "Invalid unit for conversion");
    return nullptr;
  }

  CSSNumericValue* result = to(target_unit);
  if (!result) {
    exception_state.ThrowTypeError("Cannot convert to " + unit_string);
    return nullptr;
  }

  return result;
}

CSSUnitValue* CSSNumericValue::to(CSSPrimitiveValue::UnitType unit) const {
  const auto sum = SumValue();
  if (!sum || sum->terms.size() != 1)
    return nullptr;

  const auto& term = sum->terms[0];
  if (term.units.size() == 0)
    return CSSUnitValue::Create(term.value)->ConvertTo(unit);
  if (term.units.size() == 1 && term.units.begin()->value == 1) {
    return CSSUnitValue::Create(term.value, term.units.begin()->key)
        ->ConvertTo(unit);
  }
  return nullptr;
}

CSSNumericValue* CSSNumericValue::add(
    const HeapVector<CSSNumberish>& numberishes,
    ExceptionState& exception_state) {
  auto values = CSSNumberishesToNumericValues(numberishes);
  PrependValueForArithmetic<kSumType>(values, this);

  if (CSSUnitValue* unit_value =
          MaybeSimplifyAsUnitValue(values, std::plus<double>())) {
    return unit_value;
  }
  return CSSMathSum::Create(std::move(values));
}

CSSNumericValue* CSSNumericValue::sub(
    const HeapVector<CSSNumberish>& numberishes,
    ExceptionState& exception_state) {
  auto values = CSSNumberishesToNumericValues(numberishes);
  std::transform(values.begin(), values.end(), values.begin(),
                 [](CSSNumericValue* v) { return v->Negate(); });
  PrependValueForArithmetic<kSumType>(values, this);

  if (CSSUnitValue* unit_value =
          MaybeSimplifyAsUnitValue(values, std::plus<double>())) {
    return unit_value;
  }
  return CSSMathSum::Create(std::move(values));
}

CSSNumericValue* CSSNumericValue::mul(
    const HeapVector<CSSNumberish>& numberishes,
    ExceptionState& exception_state) {
  auto values = CSSNumberishesToNumericValues(numberishes);
  PrependValueForArithmetic<kProductType>(values, this);

  if (CSSUnitValue* unit_value =
          MaybeSimplifyAsUnitValue(values, std::multiplies<double>())) {
    return unit_value;
  }
  return CSSMathProduct::Create(std::move(values));
}

CSSNumericValue* CSSNumericValue::div(
    const HeapVector<CSSNumberish>& numberishes,
    ExceptionState& exception_state) {
  auto values = CSSNumberishesToNumericValues(numberishes);
  std::transform(values.begin(), values.end(), values.begin(),
                 [](CSSNumericValue* v) { return v->Invert(); });
  PrependValueForArithmetic<kProductType>(values, this);

  if (CSSUnitValue* unit_value =
          MaybeSimplifyAsUnitValue(values, std::multiplies<double>())) {
    return unit_value;
  }
  return CSSMathProduct::Create(std::move(values));
}

CSSNumericValue* CSSNumericValue::min(
    const HeapVector<CSSNumberish>& numberishes,
    ExceptionState& exception_state) {
  auto values = CSSNumberishesToNumericValues(numberishes);
  PrependValueForArithmetic<kMinType>(values, this);

  if (CSSUnitValue* unit_value = MaybeSimplifyAsUnitValue(
          values, [](double a, double b) { return std::min(a, b); })) {
    return unit_value;
  }
  return CSSMathMin::Create(std::move(values));
}

CSSNumericValue* CSSNumericValue::max(
    const HeapVector<CSSNumberish>& numberishes,
    ExceptionState& exception_state) {
  auto values = CSSNumberishesToNumericValues(numberishes);
  PrependValueForArithmetic<kMaxType>(values, this);

  if (CSSUnitValue* unit_value = MaybeSimplifyAsUnitValue(
          values, [](double a, double b) { return std::max(a, b); })) {
    return unit_value;
  }
  return CSSMathMax::Create(std::move(values));
}

bool CSSNumericValue::equals(const HeapVector<CSSNumberish>& args) {
  CSSNumericValueVector values = CSSNumberishesToNumericValues(args);
  return std::all_of(values.begin(), values.end(),
                     [this](const auto& v) { return this->Equals(*v); });
}

CSSNumericValue* CSSNumericValue::Negate() {
  return CSSMathNegate::Create(this);
}

CSSNumericValue* CSSNumericValue::Invert() {
  return CSSMathInvert::Create(this);
}

CSSNumericValueVector CSSNumberishesToNumericValues(
    const HeapVector<CSSNumberish>& values) {
  CSSNumericValueVector result;
  for (const CSSNumberish& value : values) {
    result.push_back(CSSNumericValue::FromNumberish(value));
  }
  return result;
}

}  // namespace blink

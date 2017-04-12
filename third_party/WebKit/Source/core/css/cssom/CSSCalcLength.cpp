// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/cssom/CSSCalcLength.h"

#include "bindings/core/v8/ExceptionState.h"
#include "core/css/CSSCalculationValue.h"
#include "core/css/CSSPrimitiveValue.h"
#include "core/css/cssom/CSSCalcDictionary.h"
#include "core/css/cssom/CSSSimpleLength.h"
#include "platform/wtf/Vector.h"

namespace blink {

namespace {

static CSSPrimitiveValue::UnitType UnitFromIndex(int index) {
  DCHECK(index < CSSLengthValue::kNumSupportedUnits);
  int lowest_value = static_cast<int>(CSSPrimitiveValue::UnitType::kPercentage);
  return static_cast<CSSPrimitiveValue::UnitType>(index + lowest_value);
}

int IndexForUnit(CSSPrimitiveValue::UnitType unit) {
  return (static_cast<int>(unit) -
          static_cast<int>(CSSPrimitiveValue::UnitType::kPercentage));
}

}  // namespace

CSSCalcLength::CSSCalcLength(const CSSCalcLength& other)
    : unit_data_(other.unit_data_) {}

CSSCalcLength::CSSCalcLength(const CSSSimpleLength& other) {
  unit_data_.Set(other.LengthUnit(), other.value());
}

CSSCalcLength* CSSCalcLength::Create(const CSSLengthValue* length) {
  if (length->GetType() == kSimpleLengthType) {
    const CSSSimpleLength* simple_length = ToCSSSimpleLength(length);
    return new CSSCalcLength(*simple_length);
  }

  return new CSSCalcLength(*ToCSSCalcLength(length));
}

CSSCalcLength* CSSCalcLength::Create(const CSSCalcDictionary& dictionary,
                                     ExceptionState& exception_state) {
  int num_set = 0;
  UnitData result;

#define SET_FROM_DICT_VALUE(name, camelName, primitiveName)                    \
  if (dictionary.has##camelName()) {                                           \
    result.Set(CSSPrimitiveValue::UnitType::primitiveName, dictionary.name()); \
    num_set++;                                                                 \
  }

  SET_FROM_DICT_VALUE(px, Px, kPixels)
  SET_FROM_DICT_VALUE(percent, Percent, kPercentage)
  SET_FROM_DICT_VALUE(em, Em, kEms)
  SET_FROM_DICT_VALUE(ex, Ex, kExs)
  SET_FROM_DICT_VALUE(ch, Ch, kChs)
  SET_FROM_DICT_VALUE(rem, Rem, kRems)
  SET_FROM_DICT_VALUE(vw, Vw, kViewportWidth)
  SET_FROM_DICT_VALUE(vh, Vh, kViewportHeight)
  SET_FROM_DICT_VALUE(vmin, Vmin, kViewportMin)
  SET_FROM_DICT_VALUE(vmax, Vmax, kViewportMax)
  SET_FROM_DICT_VALUE(cm, Cm, kCentimeters)
  SET_FROM_DICT_VALUE(mm, Mm, kMillimeters)
  SET_FROM_DICT_VALUE(in, In, kInches)
  SET_FROM_DICT_VALUE(pc, Pc, kPicas)
  SET_FROM_DICT_VALUE(pt, Pt, kPoints)

#undef SET_FROM_DICT_VALUE

  if (num_set == 0) {
    exception_state.ThrowTypeError(
        "Must specify at least one value in CSSCalcDictionary for creating a "
        "CSSCalcLength.");
    return nullptr;
  }
  return new CSSCalcLength(result);
}

CSSCalcLength* CSSCalcLength::FromCSSValue(const CSSPrimitiveValue& value) {
  std::unique_ptr<UnitData> unit_data =
      UnitData::FromExpressionNode(value.CssCalcValue()->ExpressionNode());
  if (unit_data)
    return new CSSCalcLength(*unit_data);
  return nullptr;
}

CSSCalcLength* CSSCalcLength::FromLength(const Length& length) {
  DCHECK(length.IsCalculated());
  PixelsAndPercent values = length.GetPixelsAndPercent();
  UnitData unit_data;
  unit_data.Set(CSSPrimitiveValue::UnitType::kPixels, values.pixels);
  unit_data.Set(CSSPrimitiveValue::UnitType::kPercentage, values.percent);
  CSSCalcLength* result = new CSSCalcLength(unit_data);
  return result;
}

bool CSSCalcLength::ContainsPercent() const {
  return unit_data_.Has(CSSPrimitiveValue::UnitType::kPercentage);
}

CSSLengthValue* CSSCalcLength::AddInternal(const CSSLengthValue* other) {
  UnitData result = unit_data_;
  if (other->GetType() == kSimpleLengthType) {
    const CSSSimpleLength* simple_length = ToCSSSimpleLength(other);
    result.Set(
        simple_length->LengthUnit(),
        unit_data_.Get(simple_length->LengthUnit()) + simple_length->value());
  } else {
    result.Add(ToCSSCalcLength(other)->unit_data_);
  }
  return new CSSCalcLength(result);
}

CSSLengthValue* CSSCalcLength::SubtractInternal(const CSSLengthValue* other) {
  UnitData result = unit_data_;
  if (other->GetType() == kSimpleLengthType) {
    const CSSSimpleLength* simple_length = ToCSSSimpleLength(other);
    result.Set(
        simple_length->LengthUnit(),
        unit_data_.Get(simple_length->LengthUnit()) - simple_length->value());
  } else {
    result.Subtract(ToCSSCalcLength(other)->unit_data_);
  }
  return new CSSCalcLength(result);
}

CSSLengthValue* CSSCalcLength::MultiplyInternal(double x) {
  UnitData result = unit_data_;
  result.Multiply(x);
  return new CSSCalcLength(result);
}

CSSLengthValue* CSSCalcLength::DivideInternal(double x) {
  UnitData result = unit_data_;
  result.Divide(x);
  return new CSSCalcLength(result);
}

CSSValue* CSSCalcLength::ToCSSValue() const {
  CSSCalcExpressionNode* node = unit_data_.ToCSSCalcExpressionNode();
  if (node)
    return CSSPrimitiveValue::Create(CSSCalcValue::Create(node));
  return nullptr;
}

std::unique_ptr<CSSCalcLength::UnitData>
CSSCalcLength::UnitData::FromExpressionNode(
    const CSSCalcExpressionNode* expression_node) {
  CalculationCategory category = expression_node->Category();
  DCHECK(category == CalculationCategory::kCalcLength ||
         category == CalculationCategory::kCalcPercentLength ||
         category == CalculationCategory::kCalcPercent);

  if (expression_node->GetType() ==
      CSSCalcExpressionNode::Type::kCssCalcPrimitiveValue) {
    CSSPrimitiveValue::UnitType unit = expression_node->TypeWithCalcResolved();
    DCHECK(CSSLengthValue::IsSupportedLengthUnit(unit));
    std::unique_ptr<UnitData> result(new UnitData());
    result->Set(unit, expression_node->DoubleValue());
    return result;
  }

  const CSSCalcExpressionNode* left = expression_node->LeftExpressionNode();
  const CSSCalcExpressionNode* right = expression_node->RightExpressionNode();
  CalcOperator op = expression_node->OperatorType();

  if (op == kCalcMultiply) {
    std::unique_ptr<UnitData> unit_data = nullptr;
    double argument = 0;
    // One side should be a number.
    if (left->Category() == CalculationCategory::kCalcNumber) {
      unit_data = UnitData::FromExpressionNode(right);
      argument = left->DoubleValue();
    } else if (right->Category() == CalculationCategory::kCalcNumber) {
      unit_data = UnitData::FromExpressionNode(left);
      argument = right->DoubleValue();
    } else {
      NOTREACHED();
      return nullptr;
    }
    DCHECK(unit_data);
    unit_data->Multiply(argument);
    return unit_data;
  }

  if (op == kCalcDivide) {
    // Divisor must always be on the RHS.
    DCHECK_EQ(right->Category(), CalculationCategory::kCalcNumber);
    std::unique_ptr<UnitData> unit_data = UnitData::FromExpressionNode(left);
    DCHECK(unit_data);
    unit_data->Divide(right->DoubleValue());
    return unit_data;
  }

  // Add and subtract.
  std::unique_ptr<UnitData> left_unit_data = UnitData::FromExpressionNode(left);
  std::unique_ptr<UnitData> right_unit_data =
      UnitData::FromExpressionNode(right);
  DCHECK(left_unit_data);
  DCHECK(right_unit_data);

  if (op == kCalcAdd)
    left_unit_data->Add(*right_unit_data);
  else if (op == kCalcSubtract)
    left_unit_data->Subtract(*right_unit_data);
  else
    NOTREACHED();

  return left_unit_data;
}

CSSCalcExpressionNode* CSSCalcLength::UnitData::ToCSSCalcExpressionNode()
    const {
  CSSCalcExpressionNode* node = nullptr;
  for (unsigned i = 0; i < CSSLengthValue::kNumSupportedUnits; ++i) {
    if (!HasAtIndex(i))
      continue;
    double value = GetAtIndex(i);
    if (node) {
      node = CSSCalcValue::CreateExpressionNode(
          node,
          CSSCalcValue::CreateExpressionNode(
              CSSPrimitiveValue::Create(std::abs(value), UnitFromIndex(i))),
          value >= 0 ? kCalcAdd : kCalcSubtract);
    } else {
      node = CSSCalcValue::CreateExpressionNode(
          CSSPrimitiveValue::Create(value, UnitFromIndex(i)));
    }
  }
  return node;
}

bool CSSCalcLength::UnitData::HasAtIndex(int i) const {
  DCHECK_GE(i, 0);
  DCHECK_LT(i, CSSLengthValue::kNumSupportedUnits);
  return has_value_for_unit_[i];
}

bool CSSCalcLength::UnitData::Has(CSSPrimitiveValue::UnitType unit) const {
  return HasAtIndex(IndexForUnit(unit));
}

void CSSCalcLength::UnitData::SetAtIndex(int i, double value) {
  DCHECK_GE(i, 0);
  DCHECK_LT(i, CSSLengthValue::kNumSupportedUnits);
  has_value_for_unit_.set(i);
  values_[i] = value;
}

void CSSCalcLength::UnitData::Set(CSSPrimitiveValue::UnitType unit,
                                  double value) {
  SetAtIndex(IndexForUnit(unit), value);
}

double CSSCalcLength::UnitData::GetAtIndex(int i) const {
  DCHECK_GE(i, 0);
  DCHECK_LT(i, CSSLengthValue::kNumSupportedUnits);
  return values_[i];
}

double CSSCalcLength::UnitData::Get(CSSPrimitiveValue::UnitType unit) const {
  return GetAtIndex(IndexForUnit(unit));
}

void CSSCalcLength::UnitData::Add(const CSSCalcLength::UnitData& right) {
  for (int i = 0; i < CSSLengthValue::kNumSupportedUnits; ++i) {
    if (right.HasAtIndex(i)) {
      SetAtIndex(i, GetAtIndex(i) + right.GetAtIndex(i));
    }
  }
}

void CSSCalcLength::UnitData::Subtract(const CSSCalcLength::UnitData& right) {
  for (int i = 0; i < CSSLengthValue::kNumSupportedUnits; ++i) {
    if (right.HasAtIndex(i)) {
      SetAtIndex(i, GetAtIndex(i) - right.GetAtIndex(i));
    }
  }
}

void CSSCalcLength::UnitData::Multiply(double x) {
  for (int i = 0; i < CSSLengthValue::kNumSupportedUnits; ++i) {
    if (HasAtIndex(i)) {
      SetAtIndex(i, GetAtIndex(i) * x);
    }
  }
}

void CSSCalcLength::UnitData::Divide(double x) {
  DCHECK_NE(x, 0);
  for (int i = 0; i < CSSLengthValue::kNumSupportedUnits; ++i) {
    if (HasAtIndex(i)) {
      SetAtIndex(i, GetAtIndex(i) / x);
    }
  }
}

}  // namespace blink

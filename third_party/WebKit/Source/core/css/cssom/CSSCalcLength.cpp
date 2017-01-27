// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/cssom/CSSCalcLength.h"

#include "bindings/core/v8/ExceptionState.h"
#include "core/css/CSSCalculationValue.h"
#include "core/css/CSSPrimitiveValue.h"
#include "core/css/cssom/CSSCalcDictionary.h"
#include "core/css/cssom/CSSSimpleLength.h"
#include "wtf/Vector.h"

namespace blink {

namespace {

static CSSPrimitiveValue::UnitType unitFromIndex(int index) {
  DCHECK(index < CSSLengthValue::kNumSupportedUnits);
  int lowestValue = static_cast<int>(CSSPrimitiveValue::UnitType::Percentage);
  return static_cast<CSSPrimitiveValue::UnitType>(index + lowestValue);
}

int indexForUnit(CSSPrimitiveValue::UnitType unit) {
  return (static_cast<int>(unit) -
          static_cast<int>(CSSPrimitiveValue::UnitType::Percentage));
}

}  // namespace

CSSCalcLength::CSSCalcLength(const CSSCalcLength& other)
    : m_unitData(other.m_unitData) {}

CSSCalcLength::CSSCalcLength(const CSSSimpleLength& other) {
  m_unitData.set(other.lengthUnit(), other.value());
}

CSSCalcLength* CSSCalcLength::create(const CSSLengthValue* length) {
  if (length->type() == SimpleLengthType) {
    const CSSSimpleLength* simpleLength = toCSSSimpleLength(length);
    return new CSSCalcLength(*simpleLength);
  }

  return new CSSCalcLength(*toCSSCalcLength(length));
}

CSSCalcLength* CSSCalcLength::create(const CSSCalcDictionary& dictionary,
                                     ExceptionState& exceptionState) {
  int numSet = 0;
  UnitData result;

#define SET_FROM_DICT_VALUE(name, camelName, primitiveName)                    \
  if (dictionary.has##camelName()) {                                           \
    result.set(CSSPrimitiveValue::UnitType::primitiveName, dictionary.name()); \
    numSet++;                                                                  \
  }

  SET_FROM_DICT_VALUE(px, Px, Pixels)
  SET_FROM_DICT_VALUE(percent, Percent, Percentage)
  SET_FROM_DICT_VALUE(em, Em, Ems)
  SET_FROM_DICT_VALUE(ex, Ex, Exs)
  SET_FROM_DICT_VALUE(ch, Ch, Chs)
  SET_FROM_DICT_VALUE(rem, Rem, Rems)
  SET_FROM_DICT_VALUE(vw, Vw, ViewportWidth)
  SET_FROM_DICT_VALUE(vh, Vh, ViewportHeight)
  SET_FROM_DICT_VALUE(vmin, Vmin, ViewportMin)
  SET_FROM_DICT_VALUE(vmax, Vmax, ViewportMax)
  SET_FROM_DICT_VALUE(cm, Cm, Centimeters)
  SET_FROM_DICT_VALUE(mm, Mm, Millimeters)
  SET_FROM_DICT_VALUE(in, In, Inches)
  SET_FROM_DICT_VALUE(pc, Pc, Picas)
  SET_FROM_DICT_VALUE(pt, Pt, Points)

#undef SET_FROM_DICT_VALUE

  if (numSet == 0) {
    exceptionState.throwTypeError(
        "Must specify at least one value in CSSCalcDictionary for creating a "
        "CSSCalcLength.");
    return nullptr;
  }
  return new CSSCalcLength(result);
}

CSSCalcLength* CSSCalcLength::fromCSSValue(const CSSPrimitiveValue& value) {
  std::unique_ptr<UnitData> unitData =
      UnitData::fromExpressionNode(value.cssCalcValue()->expressionNode());
  if (unitData)
    return new CSSCalcLength(*unitData);
  return nullptr;
}

CSSCalcLength* CSSCalcLength::fromLength(const Length& length) {
  DCHECK(length.isCalculated());
  PixelsAndPercent values = length.getPixelsAndPercent();
  UnitData unitData;
  unitData.set(CSSPrimitiveValue::UnitType::Pixels, values.pixels);
  unitData.set(CSSPrimitiveValue::UnitType::Percentage, values.percent);
  CSSCalcLength* result = new CSSCalcLength(unitData);
  return result;
}

bool CSSCalcLength::containsPercent() const {
  return m_unitData.has(CSSPrimitiveValue::UnitType::Percentage);
}

CSSLengthValue* CSSCalcLength::addInternal(const CSSLengthValue* other) {
  UnitData result = m_unitData;
  if (other->type() == SimpleLengthType) {
    const CSSSimpleLength* simpleLength = toCSSSimpleLength(other);
    result.set(
        simpleLength->lengthUnit(),
        m_unitData.get(simpleLength->lengthUnit()) + simpleLength->value());
  } else {
    result.add(toCSSCalcLength(other)->m_unitData);
  }
  return new CSSCalcLength(result);
}

CSSLengthValue* CSSCalcLength::subtractInternal(const CSSLengthValue* other) {
  UnitData result = m_unitData;
  if (other->type() == SimpleLengthType) {
    const CSSSimpleLength* simpleLength = toCSSSimpleLength(other);
    result.set(
        simpleLength->lengthUnit(),
        m_unitData.get(simpleLength->lengthUnit()) - simpleLength->value());
  } else {
    result.subtract(toCSSCalcLength(other)->m_unitData);
  }
  return new CSSCalcLength(result);
}

CSSLengthValue* CSSCalcLength::multiplyInternal(double x) {
  UnitData result = m_unitData;
  result.multiply(x);
  return new CSSCalcLength(result);
}

CSSLengthValue* CSSCalcLength::divideInternal(double x) {
  UnitData result = m_unitData;
  result.divide(x);
  return new CSSCalcLength(result);
}

CSSValue* CSSCalcLength::toCSSValue() const {
  CSSCalcExpressionNode* node = m_unitData.toCSSCalcExpressionNode();
  if (node)
    return CSSPrimitiveValue::create(CSSCalcValue::create(node));
  return nullptr;
}

std::unique_ptr<CSSCalcLength::UnitData>
CSSCalcLength::UnitData::fromExpressionNode(
    const CSSCalcExpressionNode* expressionNode) {
  CalculationCategory category = expressionNode->category();
  DCHECK(category == CalculationCategory::CalcLength ||
         category == CalculationCategory::CalcPercentLength ||
         category == CalculationCategory::CalcPercent);

  if (expressionNode->getType() ==
      CSSCalcExpressionNode::Type::CssCalcPrimitiveValue) {
    CSSPrimitiveValue::UnitType unit = expressionNode->typeWithCalcResolved();
    DCHECK(CSSLengthValue::isSupportedLengthUnit(unit));
    std::unique_ptr<UnitData> result(new UnitData());
    result->set(unit, expressionNode->doubleValue());
    return result;
  }

  const CSSCalcExpressionNode* left = expressionNode->leftExpressionNode();
  const CSSCalcExpressionNode* right = expressionNode->rightExpressionNode();
  CalcOperator op = expressionNode->operatorType();

  if (op == CalcMultiply) {
    std::unique_ptr<UnitData> unitData = nullptr;
    double argument = 0;
    // One side should be a number.
    if (left->category() == CalculationCategory::CalcNumber) {
      unitData = UnitData::fromExpressionNode(right);
      argument = left->doubleValue();
    } else if (right->category() == CalculationCategory::CalcNumber) {
      unitData = UnitData::fromExpressionNode(left);
      argument = right->doubleValue();
    } else {
      NOTREACHED();
      return nullptr;
    }
    DCHECK(unitData);
    unitData->multiply(argument);
    return unitData;
  }

  if (op == CalcDivide) {
    // Divisor must always be on the RHS.
    DCHECK_EQ(right->category(), CalculationCategory::CalcNumber);
    std::unique_ptr<UnitData> unitData = UnitData::fromExpressionNode(left);
    DCHECK(unitData);
    unitData->divide(right->doubleValue());
    return unitData;
  }

  // Add and subtract.
  std::unique_ptr<UnitData> leftUnitData = UnitData::fromExpressionNode(left);
  std::unique_ptr<UnitData> rightUnitData = UnitData::fromExpressionNode(right);
  DCHECK(leftUnitData);
  DCHECK(rightUnitData);

  if (op == CalcAdd)
    leftUnitData->add(*rightUnitData);
  else if (op == CalcSubtract)
    leftUnitData->subtract(*rightUnitData);
  else
    NOTREACHED();

  return leftUnitData;
}

CSSCalcExpressionNode* CSSCalcLength::UnitData::toCSSCalcExpressionNode()
    const {
  CSSCalcExpressionNode* node = nullptr;
  for (unsigned i = 0; i < CSSLengthValue::kNumSupportedUnits; ++i) {
    if (!hasAtIndex(i))
      continue;
    double value = getAtIndex(i);
    if (node) {
      node = CSSCalcValue::createExpressionNode(
          node, CSSCalcValue::createExpressionNode(CSSPrimitiveValue::create(
                    std::abs(value), unitFromIndex(i))),
          value >= 0 ? CalcAdd : CalcSubtract);
    } else {
      node = CSSCalcValue::createExpressionNode(
          CSSPrimitiveValue::create(value, unitFromIndex(i)));
    }
  }
  return node;
}

bool CSSCalcLength::UnitData::hasAtIndex(int i) const {
  DCHECK_GE(i, 0);
  DCHECK_LT(i, CSSLengthValue::kNumSupportedUnits);
  return m_hasValueForUnit[i];
}

bool CSSCalcLength::UnitData::has(CSSPrimitiveValue::UnitType unit) const {
  return hasAtIndex(indexForUnit(unit));
}

void CSSCalcLength::UnitData::setAtIndex(int i, double value) {
  DCHECK_GE(i, 0);
  DCHECK_LT(i, CSSLengthValue::kNumSupportedUnits);
  m_hasValueForUnit.set(i);
  m_values[i] = value;
}

void CSSCalcLength::UnitData::set(CSSPrimitiveValue::UnitType unit,
                                  double value) {
  setAtIndex(indexForUnit(unit), value);
}

double CSSCalcLength::UnitData::getAtIndex(int i) const {
  DCHECK_GE(i, 0);
  DCHECK_LT(i, CSSLengthValue::kNumSupportedUnits);
  return m_values[i];
}

double CSSCalcLength::UnitData::get(CSSPrimitiveValue::UnitType unit) const {
  return getAtIndex(indexForUnit(unit));
}

void CSSCalcLength::UnitData::add(const CSSCalcLength::UnitData& right) {
  for (int i = 0; i < CSSLengthValue::kNumSupportedUnits; ++i) {
    if (right.hasAtIndex(i)) {
      setAtIndex(i, getAtIndex(i) + right.getAtIndex(i));
    }
  }
}

void CSSCalcLength::UnitData::subtract(const CSSCalcLength::UnitData& right) {
  for (int i = 0; i < CSSLengthValue::kNumSupportedUnits; ++i) {
    if (right.hasAtIndex(i)) {
      setAtIndex(i, getAtIndex(i) - right.getAtIndex(i));
    }
  }
}

void CSSCalcLength::UnitData::multiply(double x) {
  for (int i = 0; i < CSSLengthValue::kNumSupportedUnits; ++i) {
    if (hasAtIndex(i)) {
      setAtIndex(i, getAtIndex(i) * x);
    }
  }
}

void CSSCalcLength::UnitData::divide(double x) {
  DCHECK_NE(x, 0);
  for (int i = 0; i < CSSLengthValue::kNumSupportedUnits; ++i) {
    if (hasAtIndex(i)) {
      setAtIndex(i, getAtIndex(i) / x);
    }
  }
}

}  // namespace blink

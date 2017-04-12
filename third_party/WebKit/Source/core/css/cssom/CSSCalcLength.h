// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSSCalcLength_h
#define CSSCalcLength_h

#include <array>
#include <bitset>
#include "core/css/cssom/CSSLengthValue.h"
#include "platform/wtf/BitVector.h"

namespace blink {

class CSSCalcExpressionNode;
class CSSSimpleLength;

class CORE_EXPORT CSSCalcLength final : public CSSLengthValue {
  DEFINE_WRAPPERTYPEINFO();

 public:
  class UnitData {
   public:
    UnitData() : values_(), has_value_for_unit_() {}
    UnitData(const UnitData& other)
        : values_(other.values_),
          has_value_for_unit_(other.has_value_for_unit_) {}

    static std::unique_ptr<UnitData> FromExpressionNode(
        const CSSCalcExpressionNode*);
    CSSCalcExpressionNode* ToCSSCalcExpressionNode() const;

    bool Has(CSSPrimitiveValue::UnitType) const;
    void Set(CSSPrimitiveValue::UnitType, double);
    double Get(CSSPrimitiveValue::UnitType) const;

    void Add(const UnitData& right);
    void Subtract(const UnitData& right);
    void Multiply(double);
    void Divide(double);

   private:
    bool HasAtIndex(int) const;
    void SetAtIndex(int, double);
    double GetAtIndex(int) const;

    std::array<double, CSSLengthValue::kNumSupportedUnits> values_;
    std::bitset<CSSLengthValue::kNumSupportedUnits> has_value_for_unit_;
  };

  static CSSCalcLength* Create(const CSSCalcDictionary&, ExceptionState&);
  static CSSCalcLength* Create(const CSSLengthValue* length, ExceptionState&) {
    return Create(length);
  }
  static CSSCalcLength* Create(const CSSLengthValue*);
  static CSSCalcLength* FromCSSValue(const CSSPrimitiveValue&);

  static CSSCalcLength* FromLength(const Length&);

#define GETTER_MACRO(name, type)    \
  double name(bool& isNull) {       \
    isNull = !unit_data_.Has(type); \
    return unit_data_.Get(type);    \
  }

  GETTER_MACRO(px, CSSPrimitiveValue::UnitType::kPixels)
  GETTER_MACRO(percent, CSSPrimitiveValue::UnitType::kPercentage)
  GETTER_MACRO(em, CSSPrimitiveValue::UnitType::kEms)
  GETTER_MACRO(ex, CSSPrimitiveValue::UnitType::kExs)
  GETTER_MACRO(ch, CSSPrimitiveValue::UnitType::kChs)
  GETTER_MACRO(rem, CSSPrimitiveValue::UnitType::kRems)
  GETTER_MACRO(vw, CSSPrimitiveValue::UnitType::kViewportWidth)
  GETTER_MACRO(vh, CSSPrimitiveValue::UnitType::kViewportHeight)
  GETTER_MACRO(vmin, CSSPrimitiveValue::UnitType::kViewportMin)
  GETTER_MACRO(vmax, CSSPrimitiveValue::UnitType::kViewportMax)
  GETTER_MACRO(cm, CSSPrimitiveValue::UnitType::kCentimeters)
  GETTER_MACRO(mm, CSSPrimitiveValue::UnitType::kMillimeters)
  GETTER_MACRO(in, CSSPrimitiveValue::UnitType::kInches)
  GETTER_MACRO(pc, CSSPrimitiveValue::UnitType::kPicas)
  GETTER_MACRO(pt, CSSPrimitiveValue::UnitType::kPoints)

#undef GETTER_MACRO

  bool ContainsPercent() const override;

  CSSValue* ToCSSValue() const override;

  StyleValueType GetType() const override { return kCalcLengthType; }

 protected:
  CSSLengthValue* AddInternal(const CSSLengthValue* other) override;
  CSSLengthValue* SubtractInternal(const CSSLengthValue* other) override;
  CSSLengthValue* MultiplyInternal(double) override;
  CSSLengthValue* DivideInternal(double) override;

 private:
  CSSCalcLength();
  CSSCalcLength(const CSSCalcLength& other);
  CSSCalcLength(const CSSSimpleLength& other);
  CSSCalcLength(const UnitData& unit_data) : unit_data_(unit_data) {}

  UnitData unit_data_;
};

#define DEFINE_CALC_LENGTH_TYPE_CASTS(argumentType)                        \
  DEFINE_TYPE_CASTS(                                                       \
      CSSCalcLength, argumentType, value,                                  \
      value->GetType() == CSSLengthValue::StyleValueType::kCalcLengthType, \
      value.GetType() == CSSLengthValue::StyleValueType::kCalcLengthType)

DEFINE_CALC_LENGTH_TYPE_CASTS(CSSStyleValue);

}  // namespace blink

#endif

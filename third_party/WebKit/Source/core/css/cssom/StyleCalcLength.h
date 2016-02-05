// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef StyleCalcLength_h
#define StyleCalcLength_h

#include "core/css/cssom/LengthValue.h"
#include "wtf/BitVector.h"

namespace blink {

class CalcDictionary;
class SimpleLength;

class CORE_EXPORT StyleCalcLength final : public LengthValue {
    DEFINE_WRAPPERTYPEINFO();
public:
    static StyleCalcLength* create(const LengthValue*);
    static StyleCalcLength* create(const CalcDictionary&, ExceptionState&);
    static StyleCalcLength* create(const LengthValue* length, ExceptionState&)
    {
        return create(length);
    }

#define GETTER_MACRO(name, type) \
    double name(bool& isNull) \
    { \
        isNull = !hasAtIndex(indexForUnit(type)); \
        return get(type); \
    }

    GETTER_MACRO(px, CSSPrimitiveValue::UnitType::Pixels)
    GETTER_MACRO(percent, CSSPrimitiveValue::UnitType::Percentage)
    GETTER_MACRO(em, CSSPrimitiveValue::UnitType::Ems)
    GETTER_MACRO(ex, CSSPrimitiveValue::UnitType::Exs)
    GETTER_MACRO(ch, CSSPrimitiveValue::UnitType::Chs)
    GETTER_MACRO(rem, CSSPrimitiveValue::UnitType::Rems)
    GETTER_MACRO(vw, CSSPrimitiveValue::UnitType::ViewportWidth)
    GETTER_MACRO(vh, CSSPrimitiveValue::UnitType::ViewportHeight)
    GETTER_MACRO(vmin, CSSPrimitiveValue::UnitType::ViewportMin)
    GETTER_MACRO(vmax, CSSPrimitiveValue::UnitType::ViewportMax)
    GETTER_MACRO(cm, CSSPrimitiveValue::UnitType::Centimeters)
    GETTER_MACRO(mm, CSSPrimitiveValue::UnitType::Millimeters)
    GETTER_MACRO(in, CSSPrimitiveValue::UnitType::Inches)
    GETTER_MACRO(pc, CSSPrimitiveValue::UnitType::Picas)
    GETTER_MACRO(pt, CSSPrimitiveValue::UnitType::Points)

#undef GETTER_MACRO

    bool containsPercent() const override;

    PassRefPtrWillBeRawPtr<CSSValue> toCSSValue() const override;

    StyleValueType type() const override { return CalcLengthType; }
protected:
    LengthValue* addInternal(const LengthValue* other, ExceptionState&) override;
    LengthValue* subtractInternal(const LengthValue* other, ExceptionState&) override;
    LengthValue* multiplyInternal(double, ExceptionState&) override;
    LengthValue* divideInternal(double, ExceptionState&) override;

private:
    StyleCalcLength();
    StyleCalcLength(const StyleCalcLength& other);
    StyleCalcLength(const SimpleLength& other);

    static int indexForUnit(CSSPrimitiveValue::UnitType);
    static CSSPrimitiveValue::UnitType unitFromIndex(int index)
    {
        ASSERT(index < LengthValue::kNumSupportedUnits);
        int lowestValue = static_cast<int>(CSSPrimitiveValue::UnitType::Percentage);
        return static_cast<CSSPrimitiveValue::UnitType>(index + lowestValue);
    }

    bool hasAtIndex(int index) const
    {
        ASSERT(index < LengthValue::kNumSupportedUnits);
        return m_hasValues.quickGet(index);
    }
    bool has(CSSPrimitiveValue::UnitType unit) const { return hasAtIndex(indexForUnit(unit)); }
    void setAtIndex(double value, int index)
    {
        ASSERT(index < LengthValue::kNumSupportedUnits);
        m_hasValues.quickSet(index);
        m_values[index] = value;
    }
    void set(double value, CSSPrimitiveValue::UnitType unit) { setAtIndex(value, indexForUnit(unit)); }
    double getAtIndex(int index) const
    {
        ASSERT(index < LengthValue::kNumSupportedUnits);
        return m_values[index];
    }
    double get(CSSPrimitiveValue::UnitType unit) const { return getAtIndex(indexForUnit(unit)); }

    Vector<double, LengthValue::kNumSupportedUnits> m_values;
    BitVector m_hasValues;
};

#define DEFINE_CALC_LENGTH_TYPE_CASTS(argumentType) \
    DEFINE_TYPE_CASTS(StyleCalcLength, argumentType, value, \
        value->type() == LengthValue::StyleValueType::CalcLengthType, \
        value.type() == LengthValue::StyleValueType::CalcLengthType)

DEFINE_CALC_LENGTH_TYPE_CASTS(StyleValue);

} // namespace blink

#endif

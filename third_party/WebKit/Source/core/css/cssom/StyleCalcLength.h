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

#define GETTER_MACRO(name, index) \
    double name(bool& isNull) { isNull = !has(index); return get(index); }

    GETTER_MACRO(px, LengthUnit::Px)
    GETTER_MACRO(percent, LengthUnit::Percent)
    GETTER_MACRO(em, LengthUnit::Em)
    GETTER_MACRO(ex, LengthUnit::Ex)
    GETTER_MACRO(ch, LengthUnit::Ch)
    GETTER_MACRO(rem, LengthUnit::Rem)
    GETTER_MACRO(vw, LengthUnit::Vw)
    GETTER_MACRO(vh, LengthUnit::Vh)
    GETTER_MACRO(vmin, LengthUnit::Vmin)
    GETTER_MACRO(vmax, LengthUnit::Vmax)
    GETTER_MACRO(cm, LengthUnit::Cm)
    GETTER_MACRO(mm, LengthUnit::Mm)
    GETTER_MACRO(in, LengthUnit::In)
    GETTER_MACRO(pc, LengthUnit::Pc)
    GETTER_MACRO(pt, LengthUnit::Pt)

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

    bool has(LengthUnit lengthUnit) const
    {
        ASSERT(lengthUnit < LengthUnit::Count);
        return m_hasValues.quickGet(lengthUnit);
    }
    void set(double value, LengthUnit lengthUnit)
    {
        ASSERT(lengthUnit < LengthUnit::Count);
        m_hasValues.quickSet(lengthUnit);
        m_values[lengthUnit] = value;
    }
    double get(LengthUnit lengthUnit) const
    {
        ASSERT(lengthUnit < LengthUnit::Count);
        return m_values[lengthUnit];
    }

    Vector<double, LengthUnit::Count> m_values;
    BitVector m_hasValues;
};

#define DEFINE_CALC_LENGTH_TYPE_CASTS(argumentType) \
    DEFINE_TYPE_CASTS(StyleCalcLength, argumentType, value, \
        value->type() == LengthValue::StyleValueType::CalcLengthType, \
        value.type() == LengthValue::StyleValueType::CalcLengthType)

DEFINE_CALC_LENGTH_TYPE_CASTS(StyleValue);

} // namespace blink

#endif

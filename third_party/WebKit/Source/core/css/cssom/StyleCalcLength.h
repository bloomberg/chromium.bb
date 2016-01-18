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

#define GETTER_SETTER_MACRO(name, capsName, index) \
    double name(bool& isNull) { isNull = !has(index); return get(index); } \
    void set##capsName(double value) { set(value, index); }

    GETTER_SETTER_MACRO(px, Px, LengthUnit::Px)
    GETTER_SETTER_MACRO(percent, Percent, LengthUnit::Percent)
    GETTER_SETTER_MACRO(em, Em, LengthUnit::Em)
    GETTER_SETTER_MACRO(ex, Ex, LengthUnit::Ex)
    GETTER_SETTER_MACRO(ch, Ch, LengthUnit::Ch)
    GETTER_SETTER_MACRO(rem, Rem, LengthUnit::Rem)
    GETTER_SETTER_MACRO(vw, Vw, LengthUnit::Vw)
    GETTER_SETTER_MACRO(vh, Vh, LengthUnit::Vh)
    GETTER_SETTER_MACRO(vmin, Vmin, LengthUnit::Vmin)
    GETTER_SETTER_MACRO(vmax, Vmax, LengthUnit::Vmax)
    GETTER_SETTER_MACRO(cm, Cm, LengthUnit::Cm)
    GETTER_SETTER_MACRO(mm, Mm, LengthUnit::Mm)
    GETTER_SETTER_MACRO(in, In, LengthUnit::In)
    GETTER_SETTER_MACRO(pc, Pc, LengthUnit::Pc)
    GETTER_SETTER_MACRO(pt, Pt, LengthUnit::Pt)

#undef GETTER_SETTER_MACRO

    String cssString() const override;
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

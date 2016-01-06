// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SimpleLength_h
#define SimpleLength_h

#include "bindings/core/v8/ExceptionState.h"
#include "core/css/cssom/LengthValue.h"

namespace blink {

class CSSPrimitiveValue;

class CORE_EXPORT SimpleLength : public LengthValue {
    DEFINE_WRAPPERTYPEINFO();
public:
    static PassRefPtrWillBeRawPtr<SimpleLength> create(double value, const String& type, ExceptionState& exceptionState)
    {
        LengthUnit unit = LengthValue::lengthUnitFromName(type);
        if (unit == LengthUnit::Count) {
            exceptionState.throwTypeError("Invalid unit for SimpleLength.");
            return nullptr;
        }
        return adoptRefWillBeNoop(new SimpleLength(value, unit));
    }

    static PassRefPtrWillBeRawPtr<SimpleLength> create(double value, LengthUnit type)
    {
        return adoptRefWillBeNoop(new SimpleLength(value, type));
    }

    double value() const { return m_value; }
    String unit() const { return LengthValue::lengthTypeToString(m_unit); }
    LengthUnit lengthUnit() const { return m_unit; }

    void setValue(double value) { m_value = value; }

    StyleValueType type() const override { return StyleValueType::SimpleLengthType; }

    String cssString() const override;
    PassRefPtrWillBeRawPtr<CSSValue> toCSSValue() const override;

protected:
    SimpleLength(double value, LengthUnit unit) : LengthValue(), m_unit(unit), m_value(value) {}

    virtual PassRefPtrWillBeRawPtr<LengthValue> addInternal(const LengthValue* other, ExceptionState&);
    virtual PassRefPtrWillBeRawPtr<LengthValue> subtractInternal(const LengthValue* other, ExceptionState&);
    virtual PassRefPtrWillBeRawPtr<LengthValue> multiplyInternal(double, ExceptionState&);
    virtual PassRefPtrWillBeRawPtr<LengthValue> divideInternal(double, ExceptionState&);

    LengthUnit m_unit;
    double m_value;
};

#define DEFINE_SIMPLE_LENGTH_TYPE_CASTS(argumentType) \
    DEFINE_TYPE_CASTS(SimpleLength, argumentType, value, \
        value->type() == LengthValue::StyleValueType::SimpleLengthType, \
        value.type() == LengthValue::StyleValueType::SimpleLengthType)

DEFINE_SIMPLE_LENGTH_TYPE_CASTS(LengthValue);
DEFINE_SIMPLE_LENGTH_TYPE_CASTS(StyleValue);

} // namespace blink

#endif

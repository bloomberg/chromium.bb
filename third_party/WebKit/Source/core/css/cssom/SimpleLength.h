// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SimpleLength_h
#define SimpleLength_h

#include "bindings/core/v8/ExceptionState.h"
#include "core/css/cssom/LengthValue.h"

namespace blink {

class CSSPrimitiveValue;

class CORE_EXPORT SimpleLength final : public LengthValue {
    WTF_MAKE_NONCOPYABLE(SimpleLength);
    DEFINE_WRAPPERTYPEINFO();
public:
    static SimpleLength* create(double value, const String& type, ExceptionState& exceptionState)
    {
        LengthUnit unit = LengthValue::lengthUnitFromName(type);
        if (unit == LengthUnit::Count) {
            exceptionState.throwTypeError("Invalid unit for SimpleLength.");
            return nullptr;
        }
        return new SimpleLength(value, unit);
    }

    static SimpleLength* create(double value, LengthUnit type)
    {
        return new SimpleLength(value, type);
    }

    double value() const { return m_value; }
    String unit() const { return LengthValue::lengthTypeToString(m_unit); }
    LengthUnit lengthUnit() const { return m_unit; }

    void setValue(double value) { m_value = value; }

    StyleValueType type() const override { return StyleValueType::SimpleLengthType; }

    String cssString() const override;
    PassRefPtrWillBeRawPtr<CSSValue> toCSSValue() const override;

protected:
    virtual LengthValue* addInternal(const LengthValue* other, ExceptionState&);
    virtual LengthValue* subtractInternal(const LengthValue* other, ExceptionState&);
    virtual LengthValue* multiplyInternal(double, ExceptionState&);
    virtual LengthValue* divideInternal(double, ExceptionState&);

private:
    SimpleLength(double value, LengthUnit unit) : LengthValue(), m_unit(unit), m_value(value) {}

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

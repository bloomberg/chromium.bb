// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SimpleLength_h
#define SimpleLength_h

#include "bindings/core/v8/ExceptionState.h"
#include "core/css/cssom/CSSLengthValue.h"

namespace blink {

class CSSPrimitiveValue;

class CORE_EXPORT SimpleLength final : public CSSLengthValue {
    WTF_MAKE_NONCOPYABLE(SimpleLength);
    DEFINE_WRAPPERTYPEINFO();
public:
    static SimpleLength* create(double value, const String& type, ExceptionState& exceptionState)
    {
        CSSPrimitiveValue::UnitType unit = unitFromName(type);
        if (!isSupportedLengthUnit(unit)) {
            exceptionState.throwTypeError("Invalid unit for SimpleLength: " + type);
            return nullptr;
        }
        return new SimpleLength(value, unit);
    }

    static SimpleLength* create(double value, CSSPrimitiveValue::UnitType type)
    {
        return new SimpleLength(value, type);
    }

    bool containsPercent() const override;

    double value() const { return m_value; }
    String unit() const { return String(CSSPrimitiveValue::unitTypeToString(m_unit)); }
    CSSPrimitiveValue::UnitType lengthUnit() const { return m_unit; }

    void setValue(double value) { m_value = value; }

    StyleValueType type() const override { return StyleValueType::SimpleLengthType; }

    CSSValue* toCSSValue() const override;

protected:
    virtual CSSLengthValue* addInternal(const CSSLengthValue* other, ExceptionState&);
    virtual CSSLengthValue* subtractInternal(const CSSLengthValue* other, ExceptionState&);
    virtual CSSLengthValue* multiplyInternal(double, ExceptionState&);
    virtual CSSLengthValue* divideInternal(double, ExceptionState&);

private:
    SimpleLength(double value, CSSPrimitiveValue::UnitType unit) : CSSLengthValue(), m_unit(unit), m_value(value) {}

    CSSPrimitiveValue::UnitType m_unit;
    double m_value;
};

#define DEFINE_SIMPLE_LENGTH_TYPE_CASTS(argumentType) \
    DEFINE_TYPE_CASTS(SimpleLength, argumentType, value, \
        value->type() == CSSLengthValue::StyleValueType::SimpleLengthType, \
        value.type() == CSSLengthValue::StyleValueType::SimpleLengthType)

DEFINE_SIMPLE_LENGTH_TYPE_CASTS(CSSLengthValue);
DEFINE_SIMPLE_LENGTH_TYPE_CASTS(StyleValue);

} // namespace blink

#endif

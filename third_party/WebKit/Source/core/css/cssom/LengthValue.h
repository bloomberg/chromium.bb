// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LengthValue_h
#define LengthValue_h

#include "core/css/cssom/StyleValue.h"

namespace blink {

class CalcDictionary;
class ExceptionState;

class CORE_EXPORT LengthValue : public StyleValue {
    WTF_MAKE_NONCOPYABLE(LengthValue);
    DEFINE_WRAPPERTYPEINFO();
public:
    static CSSPrimitiveValue::UnitType unitFromName(const String& name);

    LengthValue* add(const LengthValue* other, ExceptionState&);
    LengthValue* subtract(const LengthValue* other, ExceptionState&);
    LengthValue* multiply(double, ExceptionState&);
    LengthValue* divide(double, ExceptionState&);

    virtual bool containsPercent() const = 0;

    static LengthValue* from(const String& cssString, ExceptionState&);
    static LengthValue* from(double value, const String& typeStr, ExceptionState&);
    static LengthValue* from(const CalcDictionary&, ExceptionState&);

protected:
    LengthValue() {}

    virtual LengthValue* addInternal(const LengthValue* other, ExceptionState&);
    virtual LengthValue* subtractInternal(const LengthValue* other, ExceptionState&);
    virtual LengthValue* multiplyInternal(double, ExceptionState&);
    virtual LengthValue* divideInternal(double, ExceptionState&);

    static bool isSupportedLengthUnit(CSSPrimitiveValue::UnitType unit)
    {
        return (CSSPrimitiveValue::isLength(unit) || unit == CSSPrimitiveValue::UnitType::Percentage)
            && unit != CSSPrimitiveValue::UnitType::QuirkyEms
            && unit != CSSPrimitiveValue::UnitType::UserUnits;
    }

    static const int kNumSupportedUnits = 15;
};

DEFINE_TYPE_CASTS(LengthValue, StyleValue, value,
    (value->type() == StyleValue::SimpleLengthType
        || value->type() == StyleValue::CalcLengthType),
    (value.type() == StyleValue::SimpleLengthType
        || value.type() == StyleValue::CalcLengthType));

} // namespace blink

#endif

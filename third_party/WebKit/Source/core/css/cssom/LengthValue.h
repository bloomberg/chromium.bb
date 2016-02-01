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
    enum LengthUnit {
        Px = 0,
        Percent,
        Em,
        Ex,
        Ch,
        Rem,
        Vw,
        Vh,
        Vmin,
        Vmax,
        Cm,
        Mm,
        In,
        Pc,
        Pt,
        Count // Keep last. Not a real value.
    };

    LengthValue* add(const LengthValue* other, ExceptionState&);
    LengthValue* subtract(const LengthValue* other, ExceptionState&);
    LengthValue* multiply(double, ExceptionState&);
    LengthValue* divide(double, ExceptionState&);

    virtual bool containsPercent() const = 0;

    static LengthValue* parse(const String& cssString, ExceptionState&);
    static LengthValue* fromValue(double value, const String& typeStr, ExceptionState&);
    static LengthValue* fromDictionary(const CalcDictionary&, ExceptionState&);

protected:
    LengthValue() {}

    static LengthUnit lengthUnitFromName(const String&);
    static const String& lengthTypeToString(LengthUnit);
    static CSSPrimitiveValue::UnitType lengthTypeToPrimitiveType(LengthUnit);

    virtual LengthValue* addInternal(const LengthValue* other, ExceptionState&);
    virtual LengthValue* subtractInternal(const LengthValue* other, ExceptionState&);
    virtual LengthValue* multiplyInternal(double, ExceptionState&);
    virtual LengthValue* divideInternal(double, ExceptionState&);
};

} // namespace blink

#endif

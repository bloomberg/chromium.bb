// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LengthValue_h
#define LengthValue_h

#include "core/css/cssom/StyleValue.h"

namespace blink {

class ExceptionState;

class CORE_EXPORT LengthValue : public StyleValue {
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
        QUnit,
        In,
        Pc,
        Pt,
        Count // Keep last. Not a real value.
    };

    PassRefPtrWillBeRawPtr<LengthValue> add(const LengthValue* other, ExceptionState&);
    PassRefPtrWillBeRawPtr<LengthValue> subtract(const LengthValue* other, ExceptionState&);
    PassRefPtrWillBeRawPtr<LengthValue> multiply(double, ExceptionState&);
    PassRefPtrWillBeRawPtr<LengthValue> divide(double, ExceptionState&);

    static LengthValue* parse(const String& cssString);
    static LengthValue* fromValue(double value, const String& typeStr);
    // TODO: Uncomment when Calc is implemented.
    // static LengthValue* fromDictionary(const CalcDictionary&);

protected:
    static LengthUnit lengthUnitFromName(const String&);
    static const String& lengthTypeToString(LengthUnit type);

    virtual PassRefPtrWillBeRawPtr<LengthValue> addInternal(const LengthValue* other, ExceptionState&);
    virtual PassRefPtrWillBeRawPtr<LengthValue> subtractInternal(const LengthValue* other, ExceptionState&);
    virtual PassRefPtrWillBeRawPtr<LengthValue> multiplyInternal(double, ExceptionState&);
    virtual PassRefPtrWillBeRawPtr<LengthValue> divideInternal(double, ExceptionState&);

    // Length(const String& cssString) : StyleValue(cssString) {}
    // Length(double value, LengthUnit type) {}
    LengthValue() {}
};

} // namespace blink

#endif

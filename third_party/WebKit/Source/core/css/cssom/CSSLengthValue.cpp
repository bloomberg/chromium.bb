// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/cssom/CSSLengthValue.h"

#include "bindings/core/v8/ExceptionState.h"
#include "core/css/cssom/CalcDictionary.h"
#include "core/css/cssom/SimpleLength.h"
#include "core/css/cssom/StyleCalcLength.h"
#include "wtf/HashMap.h"

namespace blink {

CSSPrimitiveValue::UnitType CSSLengthValue::unitFromName(const String& name)
{
    if (equalIgnoringASCIICase(name, "percent") || name == "%") {
        return CSSPrimitiveValue::UnitType::Percentage;
    }
    return CSSPrimitiveValue::fromName(name);
}

CSSLengthValue* CSSLengthValue::from(const String& cssString, ExceptionState& exceptionState)
{
    // TODO: Implement
    return nullptr;
}

CSSLengthValue* CSSLengthValue::from(double value, const String& type, ExceptionState&)
{
    return SimpleLength::create(value, unitFromName(type));
}

CSSLengthValue* CSSLengthValue::from(const CalcDictionary& dictionary, ExceptionState& exceptionState)
{
    return StyleCalcLength::create(dictionary, exceptionState);
}

CSSLengthValue* CSSLengthValue::add(const CSSLengthValue* other, ExceptionState& exceptionState)
{
    if (type() == other->type() || type() == CalcLengthType)
        return addInternal(other, exceptionState);

    StyleCalcLength* result = StyleCalcLength::create(this, exceptionState);
    return result->add(other, exceptionState);
}

CSSLengthValue* CSSLengthValue::subtract(const CSSLengthValue* other, ExceptionState& exceptionState)
{
    if (type() == other->type() || type() == CalcLengthType)
        return subtractInternal(other, exceptionState);

    StyleCalcLength* result = StyleCalcLength::create(this, exceptionState);
    return result->subtract(other, exceptionState);
}

CSSLengthValue* CSSLengthValue::multiply(double x, ExceptionState& exceptionState)
{
    return multiplyInternal(x, exceptionState);
}

CSSLengthValue* CSSLengthValue::divide(double x, ExceptionState& exceptionState)
{
    return divideInternal(x, exceptionState);
}

CSSLengthValue* CSSLengthValue::addInternal(const CSSLengthValue*, ExceptionState&)
{
    NOTREACHED();
    return nullptr;
}

CSSLengthValue* CSSLengthValue::subtractInternal(const CSSLengthValue*, ExceptionState&)
{
    NOTREACHED();
    return nullptr;
}

CSSLengthValue* CSSLengthValue::multiplyInternal(double, ExceptionState&)
{
    NOTREACHED();
    return nullptr;
}

CSSLengthValue* CSSLengthValue::divideInternal(double, ExceptionState&)
{
    NOTREACHED();
    return nullptr;
}

} // namespace blink

// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/cssom/LengthValue.h"

#include "bindings/core/v8/ExceptionState.h"
#include "core/css/cssom/CalcDictionary.h"
#include "core/css/cssom/SimpleLength.h"
#include "core/css/cssom/StyleCalcLength.h"
#include "wtf/HashMap.h"

namespace blink {

CSSPrimitiveValue::UnitType LengthValue::unitFromName(const String& name)
{
    if (equalIgnoringASCIICase(name, "percent") || name == "%") {
        return CSSPrimitiveValue::UnitType::Percentage;
    }
    return CSSPrimitiveValue::fromName(name);
}

LengthValue* LengthValue::parse(const String& cssString, ExceptionState& exceptionState)
{
    // TODO: Implement
    return nullptr;
}

LengthValue* LengthValue::fromValue(double value, const String& type, ExceptionState&)
{
    return SimpleLength::create(value, unitFromName(type));
}

LengthValue* LengthValue::fromDictionary(const CalcDictionary& dictionary, ExceptionState& exceptionState)
{
    return StyleCalcLength::create(dictionary, exceptionState);
}

LengthValue* LengthValue::add(const LengthValue* other, ExceptionState& exceptionState)
{
    if (type() == other->type() || type() == CalcLengthType)
        return addInternal(other, exceptionState);

    StyleCalcLength* result = StyleCalcLength::create(this, exceptionState);
    return result->add(other, exceptionState);
}

LengthValue* LengthValue::subtract(const LengthValue* other, ExceptionState& exceptionState)
{
    if (type() == other->type() || type() == CalcLengthType)
        return subtractInternal(other, exceptionState);

    StyleCalcLength* result = StyleCalcLength::create(this, exceptionState);
    return result->subtract(other, exceptionState);
}

LengthValue* LengthValue::multiply(double x, ExceptionState& exceptionState)
{
    return multiplyInternal(x, exceptionState);
}

LengthValue* LengthValue::divide(double x, ExceptionState& exceptionState)
{
    return divideInternal(x, exceptionState);
}

LengthValue* LengthValue::addInternal(const LengthValue*, ExceptionState&)
{
    ASSERT_NOT_REACHED();
    return nullptr;
}

LengthValue* LengthValue::subtractInternal(const LengthValue*, ExceptionState&)
{
    ASSERT_NOT_REACHED();
    return nullptr;
}

LengthValue* LengthValue::multiplyInternal(double, ExceptionState&)
{
    ASSERT_NOT_REACHED();
    return nullptr;
}

LengthValue* LengthValue::divideInternal(double, ExceptionState&)
{
    ASSERT_NOT_REACHED();
    return nullptr;
}

} // namespace blink

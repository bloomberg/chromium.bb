// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NumberValue_h
#define NumberValue_h

#include "bindings/core/v8/ScriptWrappable.h"
#include "core/CoreExport.h"
#include "core/css/CSSPrimitiveValue.h"
#include "core/css/cssom/StyleValue.h"
#include "wtf/text/WTFString.h"

namespace blink {

class CORE_EXPORT NumberValue final : public StyleValue {
    WTF_MAKE_NONCOPYABLE(NumberValue);
    DEFINE_WRAPPERTYPEINFO();
public:
    static NumberValue* create(double value)
    {
        return new NumberValue(value);
    }

    double value() const { return m_value; }

    PassRefPtrWillBeRawPtr<CSSValue> toCSSValue() const override
    {
        return cssValuePool().createValue(m_value, CSSPrimitiveValue::UnitType::
Number);
    }

    StyleValueType type() const override { return StyleValueType::NumberType; }
private:
    NumberValue(double value) : m_value(value) {}

    double m_value;
};

} // namespace blink

#endif

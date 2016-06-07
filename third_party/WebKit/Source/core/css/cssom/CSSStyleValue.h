// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSSStyleValue_h
#define CSSStyleValue_h

#include "bindings/core/v8/ScriptWrappable.h"
#include "core/CoreExport.h"
#include "core/css/CSSValue.h"

namespace blink {

class ExceptionState;
class ScriptState;
class ScriptValue;

class CORE_EXPORT CSSStyleValue : public GarbageCollectedFinalized<CSSStyleValue>, public ScriptWrappable {
    WTF_MAKE_NONCOPYABLE(CSSStyleValue);
    DEFINE_WRAPPERTYPEINFO();
public:
    enum StyleValueType {
        AngleType,
        CalcLengthType,
        KeywordType,
        NumberType,
        PositionType,
        SimpleLengthType,
        TransformValueType,
    };

    virtual ~CSSStyleValue() { }

    virtual StyleValueType type() const = 0;

    static ScriptValue parse(ScriptState*, const String& propertyName, const String& value, ExceptionState&);

    virtual CSSValue* toCSSValue() const = 0;
    virtual String cssString() const
    {
        return toCSSValue()->cssText();
    }

    DEFINE_INLINE_VIRTUAL_TRACE() { }

protected:
    CSSStyleValue() {}
};

} // namespace blink

#endif

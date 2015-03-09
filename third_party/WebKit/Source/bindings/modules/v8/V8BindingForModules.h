// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8BindingForModules_h
#define V8BindingForModules_h

#include "bindings/core/v8/V8Binding.h"
#include "modules/webdatabase/sqlite/SQLValue.h"

namespace blink {

template <>
struct NativeValueTraits<SQLValue> {
    static SQLValue nativeValue(const v8::Local<v8::Value>& value, v8::Isolate* isolate, ExceptionState& exceptionState)
    {
        if (value.IsEmpty() || value->IsNull())
            return SQLValue();
        if (value->IsNumber())
            return SQLValue(value->NumberValue());
        V8StringResource<> stringValue(value);
        if (!stringValue.prepare(exceptionState))
            return SQLValue();
        return SQLValue(stringValue);
    }
};

} // namespace blink

#endif // V8BindingForModules_h

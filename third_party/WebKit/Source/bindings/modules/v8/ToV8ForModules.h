// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ToV8ForModules_h
#define ToV8ForModules_h

#include "bindings/core/v8/V8Binding.h"
#include "modules/webdatabase/sqlite/SQLValue.h"

namespace blink {

inline v8::Handle<v8::Value> toV8(const SQLValue& sqlValue, v8::Handle<v8::Object> creationContext, v8::Isolate* isolate)
{
    switch (sqlValue.type()) {
    case SQLValue::NullValue:
        return v8::Null(isolate);
    case SQLValue::NumberValue:
        return v8::Number::New(isolate, sqlValue.number());
    case SQLValue::StringValue:
        return v8String(isolate, sqlValue.string());
    }
    ASSERT_NOT_REACHED();
    return v8::Local<v8::Value>();
}

} // namespace blink

#endif // ToV8ForModules_h


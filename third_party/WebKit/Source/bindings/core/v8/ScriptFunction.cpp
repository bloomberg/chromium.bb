// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "bindings/core/v8/ScriptFunction.h"

#include "bindings/core/v8/V8Binding.h"

namespace blink {

void ScriptFunction::callCallback(const v8::FunctionCallbackInfo<v8::Value>& args)
{
    v8::Isolate* isolate = args.GetIsolate();
    ASSERT(!args.Data().IsEmpty());
    ScriptFunction* function = ScriptFunction::Cast(args.Data());
    v8::Local<v8::Value> value = args.Length() > 0 ? args[0] : v8::Local<v8::Value>(v8::Undefined(isolate));

    ScriptValue result = function->call(ScriptValue(ScriptState::current(isolate), value));

    v8SetReturnValue(args, result.v8Value());
}

v8::Handle<v8::Function> ScriptFunction::adoptByGarbageCollector(PassOwnPtr<ScriptFunction> function)
{
    if (!function)
        return v8::Handle<v8::Function>();
    v8::Isolate* isolate = function->isolate();
    return createClosure(&ScriptFunction::callCallback, function.leakPtr()->releaseToV8GarbageCollector(), isolate);
}

} // namespace blink

// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "bindings/core/v8/WebGLAny.h"

#include "bindings/core/v8/ToV8.h"
#include "wtf/text/WTFString.h"

namespace blink {

ScriptValue WebGLAny(ScriptState* scriptState, bool value)
{
    return ScriptValue(scriptState, v8Boolean(value, scriptState->isolate()));
}

ScriptValue WebGLAny(ScriptState* scriptState, const bool* value, size_t size)
{
    v8::Local<v8::Array> array = v8::Array::New(scriptState->isolate(), size);
    for (size_t ii = 0; ii < size; ++ii)
        array->Set(v8::Integer::New(scriptState->isolate(), ii), v8Boolean(value[ii], scriptState->isolate()));
    return ScriptValue(scriptState, array);
}

ScriptValue WebGLAny(ScriptState* scriptState, int value)
{
    return ScriptValue(scriptState, v8::Integer::New(scriptState->isolate(), value));
}

ScriptValue WebGLAny(ScriptState* scriptState, unsigned value)
{
    return ScriptValue(scriptState, v8::Integer::NewFromUnsigned(scriptState->isolate(), static_cast<unsigned>(value)));
}

ScriptValue WebGLAny(ScriptState* scriptState, float value)
{
    return ScriptValue(scriptState, v8::Number::New(scriptState->isolate(), value));
}

ScriptValue WebGLAny(ScriptState* scriptState, String value)
{
    return ScriptValue(scriptState, v8String(scriptState->isolate(), value));
}

ScriptValue WebGLAny(ScriptState* scriptState, PassRefPtrWillBeRawPtr<WebGLObject> value)
{
    return ScriptValue(scriptState, toV8(value, scriptState->context()->Global(), scriptState->isolate()));
}

ScriptValue WebGLAny(ScriptState* scriptState, PassRefPtr<DOMFloat32Array> value)
{
    return ScriptValue(scriptState, toV8(value, scriptState->context()->Global(), scriptState->isolate()));
}

ScriptValue WebGLAny(ScriptState* scriptState, PassRefPtr<DOMInt32Array> value)
{
    return ScriptValue(scriptState, toV8(value, scriptState->context()->Global(), scriptState->isolate()));
}

ScriptValue WebGLAny(ScriptState* scriptState, PassRefPtr<DOMUint8Array> value)
{
    return ScriptValue(scriptState, toV8(value, scriptState->context()->Global(), scriptState->isolate()));
}

ScriptValue WebGLAny(ScriptState* scriptState, PassRefPtr<DOMUint32Array> value)
{
    return ScriptValue(scriptState, toV8(value, scriptState->context()->Global(), scriptState->isolate()));
}

} // namespace blink

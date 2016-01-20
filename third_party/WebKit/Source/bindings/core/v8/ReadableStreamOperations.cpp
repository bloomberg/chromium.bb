// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "bindings/core/v8/ReadableStreamOperations.h"

#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/ScriptState.h"
#include "bindings/core/v8/V8Binding.h"

namespace blink {

ScriptValue ReadableStreamOperations::getReader(ScriptState* scriptState, v8::Local<v8::Value> stream, ExceptionState& es)
{
    ASSERT(isReadableStream(scriptState, stream));

    v8::TryCatch block(scriptState->isolate());
    v8::Local<v8::Value> args[] = { stream };
    ScriptValue result(scriptState, v8CallExtra(scriptState, "AcquireReadableStreamReader", args));
    if (block.HasCaught())
        es.rethrowV8Exception(block.Exception());
    return result;
}

bool ReadableStreamOperations::isReadableStream(ScriptState* scriptState, v8::Local<v8::Value> value)
{
    if (!value->IsObject())
        return false;

    v8::Local<v8::Value> args[] = { value };
    return v8CallExtraOrCrash(scriptState, "IsReadableStream", args)->ToBoolean()->Value();
}

bool ReadableStreamOperations::isDisturbed(ScriptState* scriptState, v8::Local<v8::Value> stream)
{
    ASSERT(isReadableStream(scriptState, stream));

    v8::Local<v8::Value> args[] = { stream };
    return v8CallExtraOrCrash(scriptState, "IsReadableStreamDisturbed", args)->ToBoolean()->Value();
}

bool ReadableStreamOperations::isLocked(ScriptState* scriptState, v8::Local<v8::Value> stream)
{
    ASSERT(isReadableStream(scriptState, stream));

    v8::Local<v8::Value> args[] = { stream };
    return v8CallExtraOrCrash(scriptState, "IsReadableStreamLocked", args)->ToBoolean()->Value();
}

bool ReadableStreamOperations::isReadableStreamReader(ScriptState* scriptState, v8::Local<v8::Value> value)
{
    if (!value->IsObject())
        return false;

    v8::Local<v8::Value> args[] = { value };
    return v8CallExtraOrCrash(scriptState, "IsReadableStreamReader", args)->ToBoolean()->Value();
}

ScriptPromise ReadableStreamOperations::read(ScriptState* scriptState, v8::Local<v8::Value> reader)
{
    ASSERT(isReadableStreamReader(scriptState, reader));

    v8::Local<v8::Value> args[] = { reader };
    return ScriptPromise::cast(scriptState, v8CallExtraOrCrash(scriptState, "ReadFromReadableStreamReader", args));
}

} // namespace blink


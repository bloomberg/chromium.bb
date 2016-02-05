// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "bindings/core/v8/ReadableStreamOperations.h"

#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/ScriptState.h"
#include "bindings/core/v8/V8ScriptRunner.h"

namespace blink {

ScriptValue ReadableStreamOperations::getReader(ScriptState* scriptState, ScriptValue stream, ExceptionState& es)
{
    ASSERT(isReadableStream(scriptState, stream));

    v8::TryCatch block(scriptState->isolate());
    v8::Local<v8::Value> args[] = { stream.v8Value() };
    ScriptValue result(scriptState, V8ScriptRunner::callExtra(scriptState, "AcquireReadableStreamReader", args));
    if (block.HasCaught())
        es.rethrowV8Exception(block.Exception());
    return result;
}

bool ReadableStreamOperations::isReadableStream(ScriptState* scriptState, ScriptValue value)
{
    ASSERT(!value.isEmpty());

    if (!value.isObject())
        return false;

    v8::Local<v8::Value> args[] = { value.v8Value() };
    return V8ScriptRunner::callExtraOrCrash(scriptState, "IsReadableStream", args)->ToBoolean()->Value();
}

bool ReadableStreamOperations::isDisturbed(ScriptState* scriptState, ScriptValue stream)
{
    ASSERT(isReadableStream(scriptState, stream));

    v8::Local<v8::Value> args[] = { stream.v8Value() };
    return V8ScriptRunner::callExtraOrCrash(scriptState, "IsReadableStreamDisturbed", args)->ToBoolean()->Value();
}

bool ReadableStreamOperations::isLocked(ScriptState* scriptState, ScriptValue stream)
{
    ASSERT(isReadableStream(scriptState, stream));

    v8::Local<v8::Value> args[] = { stream.v8Value() };
    return V8ScriptRunner::callExtraOrCrash(scriptState, "IsReadableStreamLocked", args)->ToBoolean()->Value();
}

bool ReadableStreamOperations::isReadableStreamReader(ScriptState* scriptState, ScriptValue value)
{
    ASSERT(!value.isEmpty());

    if (!value.isObject())
        return false;

    v8::Local<v8::Value> args[] = { value.v8Value() };
    return V8ScriptRunner::callExtraOrCrash(scriptState, "IsReadableStreamReader", args)->ToBoolean()->Value();
}

ScriptPromise ReadableStreamOperations::read(ScriptState* scriptState, ScriptValue reader)
{
    ASSERT(isReadableStreamReader(scriptState, reader));

    v8::Local<v8::Value> args[] = { reader.v8Value() };
    return ScriptPromise::cast(scriptState, V8ScriptRunner::callExtraOrCrash(scriptState, "ReadFromReadableStreamReader", args));
}

} // namespace blink


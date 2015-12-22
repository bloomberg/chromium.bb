// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "bindings/core/v8/ReadableStreamOperations.h"

#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/ScriptState.h"
#include "bindings/core/v8/V8BindingMacros.h"

namespace blink {

namespace {

v8::MaybeLocal<v8::Value> call(ScriptState* scriptState, const char* name, size_t numArgs, v8::Local<v8::Value>* args)
{
    v8::Isolate* isolate = scriptState->isolate();
    v8::Local<v8::Context> context = scriptState->context();
    v8::Local<v8::Value> undefined = v8::Undefined(isolate);
    v8::Local<v8::Value> functionValue = scriptState->getFromExtrasExports(name).v8Value();
    ASSERT(!functionValue.IsEmpty() && functionValue->IsFunction());
    v8::Local<v8::Function> function = functionValue.As<v8::Function>();
    return function->Call(context, undefined, numArgs, args);
}

template <size_t N>
v8::MaybeLocal<v8::Value> call(ScriptState* scriptState, const char* name, v8::Local<v8::Value>(&args)[N])
{
    return call(scriptState, name, N, args);
}

} // namespace

ScriptValue ReadableStreamOperations::getReader(ScriptState* scriptState, v8::Local<v8::Value> stream, ExceptionState& es)
{
    ASSERT(isReadableStream(scriptState, stream));

    v8::TryCatch block(scriptState->isolate());
    v8::Local<v8::Value> args[] = { stream };
    ScriptValue result(scriptState, call(scriptState, "AcquireReadableStreamReader", args));
    if (block.HasCaught())
        es.rethrowV8Exception(block.Exception());
    return result;
}

bool ReadableStreamOperations::isReadableStream(ScriptState* scriptState, v8::Local<v8::Value> value)
{
    if (!value->IsObject())
        return false;

    v8::Local<v8::Value> args[] = { value };
    return v8CallOrCrash(call(scriptState, "IsReadableStream", args))->ToBoolean()->Value();
}

bool ReadableStreamOperations::isDisturbed(ScriptState* scriptState, v8::Local<v8::Value> stream)
{
    ASSERT(isReadableStream(scriptState, stream));

    v8::Local<v8::Value> args[] = { stream };
    return v8CallOrCrash(call(scriptState, "IsReadableStreamDisturbed", args))->ToBoolean()->Value();
}

bool ReadableStreamOperations::isLocked(ScriptState* scriptState, v8::Local<v8::Value> stream)
{
    ASSERT(isReadableStream(scriptState, stream));

    v8::Local<v8::Value> args[] = { stream };
    return v8CallOrCrash(call(scriptState, "IsReadableStreamLocked", args))->ToBoolean()->Value();
}

bool ReadableStreamOperations::isReadableStreamReader(ScriptState* scriptState, v8::Local<v8::Value> value)
{
    if (!value->IsObject())
        return false;

    v8::Local<v8::Value> args[] = { value };
    return v8CallOrCrash(call(scriptState, "IsReadableStreamReader", args))->ToBoolean()->Value();
}

ScriptPromise ReadableStreamOperations::read(ScriptState* scriptState, v8::Local<v8::Value> reader)
{
    ASSERT(isReadableStreamReader(scriptState, reader));

    v8::Local<v8::Value> args[] = { reader };
    return ScriptPromise::cast(scriptState, v8CallOrCrash(call(scriptState, "ReadFromReadableStreamReader", args)));
}

} // namespace blink


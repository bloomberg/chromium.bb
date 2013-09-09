/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "bindings/v8/V8ScriptRunner.h"

#include "bindings/v8/V8Binding.h"
#include "bindings/v8/V8GCController.h"
#include "bindings/v8/V8RecursionScope.h"
#include "core/dom/ScriptExecutionContext.h"
#include "core/fetch/CachedMetadata.h"
#include "core/fetch/ScriptResource.h"
#include "core/platform/chromium/TraceEvent.h"

namespace WebCore {

PassOwnPtr<v8::ScriptData> V8ScriptRunner::precompileScript(v8::Handle<v8::String> code, ScriptResource* resource)
{
    TRACE_EVENT0("v8", "v8.compile");
    TRACE_EVENT_SCOPED_SAMPLING_STATE("V8", "Compile");
    // A pseudo-randomly chosen ID used to store and retrieve V8 ScriptData from
    // the ScriptResource. If the format changes, this ID should be changed too.
    static const unsigned dataTypeID = 0xECC13BD7;

    // Very small scripts are not worth the effort to preparse.
    static const int minPreparseLength = 1024;

    if (!resource || code->Length() < minPreparseLength)
        return nullptr;

    CachedMetadata* cachedMetadata = resource->cachedMetadata(dataTypeID);
    if (cachedMetadata)
        return adoptPtr(v8::ScriptData::New(cachedMetadata->data(), cachedMetadata->size()));

    OwnPtr<v8::ScriptData> scriptData = adoptPtr(v8::ScriptData::PreCompile(code));
    if (!scriptData)
        return nullptr;

    resource->setCachedMetadata(dataTypeID, scriptData->Data(), scriptData->Length());

    return scriptData.release();
}

v8::Local<v8::Script> V8ScriptRunner::compileScript(v8::Handle<v8::String> code, const String& fileName, const TextPosition& scriptStartPosition, v8::ScriptData* scriptData, v8::Isolate* isolate, AccessControlStatus corsStatus)
{
    TRACE_EVENT0("v8", "v8.compile");
    TRACE_EVENT_SCOPED_SAMPLING_STATE("V8", "Compile");
    v8::Handle<v8::String> name = v8String(fileName, isolate);
    v8::Handle<v8::Integer> line = v8::Integer::New(scriptStartPosition.m_line.zeroBasedInt(), isolate);
    v8::Handle<v8::Integer> column = v8::Integer::New(scriptStartPosition.m_column.zeroBasedInt(), isolate);
    v8::Handle<v8::Boolean> isSharedCrossOrigin = corsStatus == SharableCrossOrigin ? v8::True() : v8::False();
    v8::ScriptOrigin origin(name, line, column, isSharedCrossOrigin);
    return v8::Script::Compile(code, &origin, scriptData);
}

v8::Local<v8::Value> V8ScriptRunner::runCompiledScript(v8::Handle<v8::Script> script, ScriptExecutionContext* context, v8::Isolate* isolate)
{
    TRACE_EVENT0("v8", "v8.run");
    TRACE_EVENT_SCOPED_SAMPLING_STATE("V8", "Execution");
    if (script.IsEmpty())
        return v8::Local<v8::Value>();

    if (V8RecursionScope::recursionLevel() >= kMaxRecursionDepth)
        return handleMaxRecursionDepthExceeded(isolate);

    if (handleOutOfMemory())
        return v8::Local<v8::Value>();

    // Run the script and keep track of the current recursion depth.
    v8::Local<v8::Value> result;
    {
        V8RecursionScope recursionScope(context);
        result = script->Run();
    }

    if (handleOutOfMemory())
        ASSERT(result.IsEmpty());

    if (result.IsEmpty())
        return v8::Local<v8::Value>();

    crashIfV8IsDead();
    return result;
}

v8::Local<v8::Value> V8ScriptRunner::compileAndRunInternalScript(v8::Handle<v8::String> source, v8::Isolate* isolate, const String& fileName, const TextPosition& scriptStartPosition, v8::ScriptData* scriptData)
{
    TRACE_EVENT0("v8", "v8.run");
    TRACE_EVENT_SCOPED_SAMPLING_STATE("V8", "Execution");
    v8::Handle<v8::Script> script = V8ScriptRunner::compileScript(source, fileName, scriptStartPosition, scriptData, isolate);
    if (script.IsEmpty())
        return v8::Local<v8::Value>();

    V8RecursionScope::MicrotaskSuppression recursionScope;
    v8::Local<v8::Value> result = script->Run();
    crashIfV8IsDead();
    return result;
}

v8::Local<v8::Value> V8ScriptRunner::callFunction(v8::Handle<v8::Function> function, ScriptExecutionContext* context, v8::Handle<v8::Object> receiver, int argc, v8::Handle<v8::Value> args[], v8::Isolate* isolate)
{
    TRACE_EVENT0("v8", "v8.callFunction");
    TRACE_EVENT_SCOPED_SAMPLING_STATE("V8", "Execution");

    if (V8RecursionScope::recursionLevel() >= kMaxRecursionDepth)
        return handleMaxRecursionDepthExceeded(isolate);

    V8RecursionScope recursionScope(context);
    v8::Local<v8::Value> result = function->Call(receiver, argc, args);
    crashIfV8IsDead();
    return result;
}

v8::Local<v8::Value> V8ScriptRunner::callInternalFunction(v8::Handle<v8::Function> function, v8::Handle<v8::Object> receiver, int argc, v8::Handle<v8::Value> args[], v8::Isolate* isolate)
{
    TRACE_EVENT0("v8", "v8.callFunction");
    TRACE_EVENT_SCOPED_SAMPLING_STATE("V8", "Execution");
    V8RecursionScope::MicrotaskSuppression recursionScope;
    v8::Local<v8::Value> result = function->Call(receiver, argc, args);
    crashIfV8IsDead();
    return result;
}

v8::Local<v8::Value> V8ScriptRunner::callAsFunction(v8::Handle<v8::Object> object, v8::Handle<v8::Object> receiver, int argc, v8::Handle<v8::Value> args[])
{
    TRACE_EVENT0("v8", "v8.callFunction");
    TRACE_EVENT_SCOPED_SAMPLING_STATE("V8", "Execution");
    V8RecursionScope::MicrotaskSuppression recursionScope;
    v8::Local<v8::Value> result = object->CallAsFunction(receiver, argc, args);
    crashIfV8IsDead();
    return result;
}

v8::Local<v8::Value> V8ScriptRunner::callAsConstructor(v8::Handle<v8::Object> object, int argc, v8::Handle<v8::Value> args[])
{
    TRACE_EVENT0("v8", "v8.callFunction");
    TRACE_EVENT_SCOPED_SAMPLING_STATE("V8", "Execution");
    V8RecursionScope::MicrotaskSuppression recursionScope;
    v8::Local<v8::Value> result = object->CallAsConstructor(argc, args);
    crashIfV8IsDead();
    return result;
}

v8::Local<v8::Object> V8ScriptRunner::instantiateObject(v8::Handle<v8::ObjectTemplate> objectTemplate)
{
    TRACE_EVENT0("v8", "v8.newInstance");
    TRACE_EVENT_SCOPED_SAMPLING_STATE("V8", "Execution");
    V8RecursionScope::MicrotaskSuppression scope;
    v8::Local<v8::Object> result = objectTemplate->NewInstance();
    crashIfV8IsDead();
    return result;
}

v8::Local<v8::Object> V8ScriptRunner::instantiateObject(v8::Handle<v8::Function> function, int argc, v8::Handle<v8::Value> argv[])
{
    TRACE_EVENT0("v8", "v8.newInstance");
    TRACE_EVENT_SCOPED_SAMPLING_STATE("V8", "Execution");
    V8RecursionScope::MicrotaskSuppression scope;
    v8::Local<v8::Object> result = function->NewInstance(argc, argv);
    crashIfV8IsDead();
    return result;
}

v8::Local<v8::Object> V8ScriptRunner::instantiateObjectInDocument(v8::Handle<v8::Function> function, ScriptExecutionContext* context, int argc, v8::Handle<v8::Value> argv[])
{
    TRACE_EVENT0("v8", "v8.newInstance");
    TRACE_EVENT_SCOPED_SAMPLING_STATE("V8", "Execution");
    V8RecursionScope scope(context);
    v8::Local<v8::Object> result = function->NewInstance(argc, argv);
    crashIfV8IsDead();
    return result;
}

} // namespace WebCore

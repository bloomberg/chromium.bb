// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "bindings/core/v8/PrivateScriptRunner.h"

#include "bindings/core/v8/DOMWrapperWorld.h"
#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/V8Binding.h"
#include "bindings/core/v8/V8PerContextData.h"
#include "bindings/core/v8/V8ScriptRunner.h"
#include "core/PrivateScriptSources.h"
#ifndef NDEBUG
#include "core/PrivateScriptSourcesForTesting.h"
#endif

namespace WebCore {

static v8::Handle<v8::Value> compilePrivateScript(v8::Isolate* isolate, String className)
{
    size_t index;
#ifndef NDEBUG
    for (index = 0; index < WTF_ARRAY_LENGTH(kPrivateScriptSourcesForTesting); index++) {
        if (className == kPrivateScriptSourcesForTesting[index].name)
            break;
    }
    if (index != WTF_ARRAY_LENGTH(kPrivateScriptSourcesForTesting)) {
        String source(reinterpret_cast<const char*>(kPrivateScriptSourcesForTesting[index].source), kPrivateScriptSourcesForTesting[index].size);
        return V8ScriptRunner::compileAndRunInternalScript(v8String(isolate, source), isolate);
    }
#endif

    // |kPrivateScriptSources| is defined in V8PrivateScriptSources.h, which is auto-generated
    // by make_private_script.py.
    for (index = 0; index < WTF_ARRAY_LENGTH(kPrivateScriptSources); index++) {
        if (className == kPrivateScriptSources[index].name)
            break;
    }
    RELEASE_ASSERT(index != WTF_ARRAY_LENGTH(kPrivateScriptSources));
    String source(reinterpret_cast<const char*>(kPrivateScriptSources[index].source), kPrivateScriptSources[index].size);
    return V8ScriptRunner::compileAndRunInternalScript(v8String(isolate, source), isolate);
}

static v8::Handle<v8::Object> classObjectOfPrivateScript(ScriptState* scriptState, String className)
{
    ASSERT(scriptState->perContextData());
    ASSERT(scriptState->executionContext());
    v8::Isolate* isolate = scriptState->isolate();
    v8::Handle<v8::Value> compiledClass = scriptState->perContextData()->compiledPrivateScript(className);
    if (compiledClass.IsEmpty()) {
        v8::Handle<v8::Value> installedClasses = scriptState->perContextData()->compiledPrivateScript("PrivateScriptRunner");
        if (installedClasses.IsEmpty()) {
            installedClasses = compilePrivateScript(isolate, "PrivateScriptRunner");
            scriptState->perContextData()->setCompiledPrivateScript("PrivateScriptRunner", installedClasses);
        }
        RELEASE_ASSERT(!installedClasses.IsEmpty());
        RELEASE_ASSERT(installedClasses->IsObject());

        compilePrivateScript(isolate, className);
        compiledClass = v8::Handle<v8::Object>::Cast(installedClasses)->Get(v8String(isolate, className));
        RELEASE_ASSERT(!compiledClass.IsEmpty());
        RELEASE_ASSERT(compiledClass->IsObject());
        scriptState->perContextData()->setCompiledPrivateScript(className, compiledClass);
    }
    return v8::Handle<v8::Object>::Cast(compiledClass);
}

// FIXME: Replace this method with a V8 API for getOwnPropertyDescriptor, once V8 is rolled.
static v8::Handle<v8::Object> getOwnPropertyDescriptor(ScriptState* scriptState, v8::Handle<v8::Object> classObject, String name)
{
    ASSERT(!scriptState->contextIsEmpty());
    v8::Handle<v8::Value> object = scriptState->context()->Global()->Get(v8String(scriptState->isolate(), "Object"));
    RELEASE_ASSERT(!object.IsEmpty());
    RELEASE_ASSERT(object->IsObject());
    v8::Handle<v8::Value> getOwnPropertyDescriptorFunction = v8::Handle<v8::Object>::Cast(object)->Get(v8String(scriptState->isolate(), "getOwnPropertyDescriptor"));
    RELEASE_ASSERT(!getOwnPropertyDescriptorFunction.IsEmpty());
    RELEASE_ASSERT(getOwnPropertyDescriptorFunction->IsFunction());
    v8::Handle<v8::Value> argv[] = { classObject, v8String(scriptState->isolate(), name) };
    v8::Handle<v8::Value> descriptor = V8ScriptRunner::callInternalFunction(v8::Handle<v8::Function>::Cast(getOwnPropertyDescriptorFunction), object, WTF_ARRAY_LENGTH(argv), argv, scriptState->isolate());
    RELEASE_ASSERT(!descriptor.IsEmpty());
    RELEASE_ASSERT(descriptor->IsObject());
    return v8::Handle<v8::Object>::Cast(descriptor);
}

static void initializeHolderIfNeeded(ScriptState* scriptState, v8::Handle<v8::Object> classObject, v8::Handle<v8::Value> holder)
{
    RELEASE_ASSERT(!holder.IsEmpty());
    RELEASE_ASSERT(holder->IsObject());
    v8::Handle<v8::Object> holderObject = v8::Handle<v8::Object>::Cast(holder);
    v8::Isolate* isolate = scriptState->isolate();
    v8::Handle<v8::Value> isInitialized = V8HiddenValue::getHiddenValue(isolate, holderObject, V8HiddenValue::privateScriptObjectIsInitialized(isolate));
    if (isInitialized.IsEmpty()) {
        v8::Handle<v8::Value> initializeFunction = classObject->Get(v8String(isolate, "constructor"));
        if (!initializeFunction.IsEmpty() && initializeFunction->IsFunction()) {
            V8ScriptRunner::callFunction(v8::Handle<v8::Function>::Cast(initializeFunction), scriptState->executionContext(), holder, 0, 0, isolate);
        }
        isInitialized = v8Boolean(true, isolate);
        V8HiddenValue::setHiddenValue(isolate, holderObject, V8HiddenValue::privateScriptObjectIsInitialized(isolate), isInitialized);
    }
}

v8::Handle<v8::Value> PrivateScriptRunner::installClass(LocalFrame* frame, String className)
{
    if (!frame)
        return v8::Handle<v8::Value>();
    v8::Handle<v8::Context> context = toV8Context(frame, DOMWrapperWorld::privateScriptIsolatedWorld());
    if (context.IsEmpty())
        return v8::Handle<v8::Value>();
    ScriptState* scriptState = ScriptState::from(context);
    if (!scriptState->executionContext())
        return v8::Handle<v8::Value>();

    ScriptState::Scope scope(scriptState);
    return classObjectOfPrivateScript(scriptState, className);
}

v8::Handle<v8::Value> PrivateScriptRunner::runDOMAttributeGetter(ScriptState* scriptState, String className, String attributeName, v8::Handle<v8::Value> holder)
{
    v8::Handle<v8::Object> classObject = classObjectOfPrivateScript(scriptState, className);
    v8::Handle<v8::Object> descriptor = getOwnPropertyDescriptor(scriptState, classObject, attributeName);
    v8::Handle<v8::Value> getter = descriptor->Get(v8String(scriptState->isolate(), "get"));
    RELEASE_ASSERT(!getter.IsEmpty());
    RELEASE_ASSERT(getter->IsFunction());
    initializeHolderIfNeeded(scriptState, classObject, holder);
    return V8ScriptRunner::callFunction(v8::Handle<v8::Function>::Cast(getter), scriptState->executionContext(), holder, 0, 0, scriptState->isolate());
}

void PrivateScriptRunner::runDOMAttributeSetter(ScriptState* scriptState, String className, String attributeName, v8::Handle<v8::Value> holder, v8::Handle<v8::Value> v8Value)
{
    v8::Handle<v8::Object> classObject = classObjectOfPrivateScript(scriptState, className);
    v8::Handle<v8::Object> descriptor = getOwnPropertyDescriptor(scriptState, classObject, attributeName);
    v8::Handle<v8::Value> setter = descriptor->Get(v8String(scriptState->isolate(), "set"));
    RELEASE_ASSERT(!setter.IsEmpty());
    RELEASE_ASSERT(setter->IsFunction());
    initializeHolderIfNeeded(scriptState, classObject, holder);
    v8::Handle<v8::Value> argv[] = { v8Value };
    V8ScriptRunner::callFunction(v8::Handle<v8::Function>::Cast(setter), scriptState->executionContext(), holder, WTF_ARRAY_LENGTH(argv), argv, scriptState->isolate());
}

v8::Handle<v8::Value> PrivateScriptRunner::runDOMMethod(ScriptState* scriptState, String className, String methodName, v8::Handle<v8::Value> holder, int argc, v8::Handle<v8::Value> argv[])
{
    v8::Handle<v8::Object> classObject = classObjectOfPrivateScript(scriptState, className);
    v8::Handle<v8::Value> method = classObject->Get(v8String(scriptState->isolate(), methodName));
    RELEASE_ASSERT(!method.IsEmpty());
    RELEASE_ASSERT(method->IsFunction());
    initializeHolderIfNeeded(scriptState, classObject, holder);
    return V8ScriptRunner::callFunction(v8::Handle<v8::Function>::Cast(method), scriptState->executionContext(), holder, argc, argv, scriptState->isolate());
}

bool PrivateScriptRunner::throwDOMExceptionInPrivateScriptIfNeeded(v8::Isolate* isolate, ExceptionState& exceptionState, v8::Handle<v8::Value> exception)
{
    if (exception.IsEmpty() || !exception->IsObject())
        return false;

    v8::Handle<v8::Object> exceptionObject = v8::Handle<v8::Object>::Cast(exception);
    v8::Handle<v8::Value> type = exceptionObject->Get(v8String(isolate, "type"));
    if (type.IsEmpty() || !type->IsString())
        return false;
    if (toCoreString(v8::Handle<v8::String>::Cast(type)) != "DOMExceptionInPrivateScript")
        return false;

    v8::Handle<v8::Value> message = exceptionObject->Get(v8String(isolate, "message"));
    RELEASE_ASSERT(!message.IsEmpty() && message->IsString());
    v8::Handle<v8::Value> code = exceptionObject->Get(v8String(isolate, "code"));
    RELEASE_ASSERT(!code.IsEmpty() && code->IsInt32());
    // FIXME: Support JavaScript errors such as TypeError, RangeError and SecurityError.
    exceptionState.throwDOMException(toInt32(code), toCoreString(v8::Handle<v8::String>::Cast(message)));
    exceptionState.throwIfNeeded();
    return true;
}

} // namespace WebCore

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
#include "core/dom/ExceptionCode.h"

namespace blink {

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
    if (index == WTF_ARRAY_LENGTH(kPrivateScriptSources)) {
        WTF_LOG_ERROR("Private script error: Target source code was not found. (Class name = %s)\n", className.utf8().data());
        RELEASE_ASSERT_NOT_REACHED();
    }

    v8::TryCatch block;
    String source(reinterpret_cast<const char*>(kPrivateScriptSources[index].source), kPrivateScriptSources[index].size);
    v8::Handle<v8::Value> result = V8ScriptRunner::compileAndRunInternalScript(v8String(isolate, source), isolate);
    if (block.HasCaught()) {
        WTF_LOG_ERROR("Private script error: Compile failed. (Class name = %s)\n", className.utf8().data());
        if (!block.Message().IsEmpty())
            WTF_LOG_ERROR("%s\n", toCoreString(block.Message()->Get()).utf8().data());
        RELEASE_ASSERT_NOT_REACHED();
    }
    return result;
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

static void initializeHolderIfNeeded(ScriptState* scriptState, v8::Handle<v8::Object> classObject, v8::Handle<v8::Value> holder)
{
    RELEASE_ASSERT(!holder.IsEmpty());
    RELEASE_ASSERT(holder->IsObject());
    v8::Handle<v8::Object> holderObject = v8::Handle<v8::Object>::Cast(holder);
    v8::Isolate* isolate = scriptState->isolate();
    v8::Handle<v8::Value> isInitialized = V8HiddenValue::getHiddenValue(isolate, holderObject, V8HiddenValue::privateScriptObjectIsInitialized(isolate));
    if (isInitialized.IsEmpty()) {
        v8::TryCatch block;
        v8::Handle<v8::Value> initializeFunction = classObject->Get(v8String(isolate, "initialize"));
        if (!initializeFunction.IsEmpty() && initializeFunction->IsFunction()) {
            v8::TryCatch block;
            V8ScriptRunner::callFunction(v8::Handle<v8::Function>::Cast(initializeFunction), scriptState->executionContext(), holder, 0, 0, isolate);
            if (block.HasCaught()) {
                WTF_LOG_ERROR("Private script error: Object constructor threw an exception.\n");
                if (!block.Message().IsEmpty())
                    WTF_LOG_ERROR("%s\n", toCoreString(block.Message()->Get()).utf8().data());
                RELEASE_ASSERT_NOT_REACHED();
            }
        }

        // Inject the prototype object of the private script into the prototype chain of the holder object.
        // This is necessary to let the holder object use properties defined on the prototype object
        // of the private script. (e.g., if the prototype object has |foo|, the holder object should be able
        // to use it with |this.foo|.)
        if (classObject->GetPrototype() != holderObject->GetPrototype())
            classObject->SetPrototype(holderObject->GetPrototype());
        holderObject->SetPrototype(classObject);

        isInitialized = v8Boolean(true, isolate);
        V8HiddenValue::setHiddenValue(isolate, holderObject, V8HiddenValue::privateScriptObjectIsInitialized(isolate), isInitialized);
    }
}

v8::Handle<v8::Value> PrivateScriptRunner::installClassIfNeeded(LocalFrame* frame, String className)
{
    if (!frame)
        return v8::Handle<v8::Value>();
    v8::HandleScope handleScope(toIsolate(frame));
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
    v8::Handle<v8::Value> descriptor = classObject->GetOwnPropertyDescriptor(v8String(scriptState->isolate(), attributeName));
    if (descriptor.IsEmpty() || !descriptor->IsObject()) {
        WTF_LOG_ERROR("Private script error: Target DOM attribute getter was not found. (Class name = %s, Attribute name = %s)\n", className.utf8().data(), attributeName.utf8().data());
        RELEASE_ASSERT_NOT_REACHED();
    }
    v8::Handle<v8::Value> getter = v8::Handle<v8::Object>::Cast(descriptor)->Get(v8String(scriptState->isolate(), "get"));
    if (getter.IsEmpty() || !getter->IsFunction()) {
        WTF_LOG_ERROR("Private script error: Target DOM attribute getter was not found. (Class name = %s, Attribute name = %s)\n", className.utf8().data(), attributeName.utf8().data());
        RELEASE_ASSERT_NOT_REACHED();
    }
    initializeHolderIfNeeded(scriptState, classObject, holder);
    return V8ScriptRunner::callFunction(v8::Handle<v8::Function>::Cast(getter), scriptState->executionContext(), holder, 0, 0, scriptState->isolate());
}

void PrivateScriptRunner::runDOMAttributeSetter(ScriptState* scriptState, String className, String attributeName, v8::Handle<v8::Value> holder, v8::Handle<v8::Value> v8Value)
{
    v8::Handle<v8::Object> classObject = classObjectOfPrivateScript(scriptState, className);
    v8::Handle<v8::Value> descriptor = classObject->GetOwnPropertyDescriptor(v8String(scriptState->isolate(), attributeName));
    if (descriptor.IsEmpty() || !descriptor->IsObject()) {
        WTF_LOG_ERROR("Private script error: Target DOM attribute setter was not found. (Class name = %s, Attribute name = %s)\n", className.utf8().data(), attributeName.utf8().data());
        RELEASE_ASSERT_NOT_REACHED();
    }
    v8::Handle<v8::Value> setter = v8::Handle<v8::Object>::Cast(descriptor)->Get(v8String(scriptState->isolate(), "set"));
    if (setter.IsEmpty() || !setter->IsFunction()) {
        WTF_LOG_ERROR("Private script error: Target DOM attribute setter was not found. (Class name = %s, Attribute name = %s)\n", className.utf8().data(), attributeName.utf8().data());
        RELEASE_ASSERT_NOT_REACHED();
    }
    initializeHolderIfNeeded(scriptState, classObject, holder);
    v8::Handle<v8::Value> argv[] = { v8Value };
    V8ScriptRunner::callFunction(v8::Handle<v8::Function>::Cast(setter), scriptState->executionContext(), holder, WTF_ARRAY_LENGTH(argv), argv, scriptState->isolate());
}

v8::Handle<v8::Value> PrivateScriptRunner::runDOMMethod(ScriptState* scriptState, String className, String methodName, v8::Handle<v8::Value> holder, int argc, v8::Handle<v8::Value> argv[])
{
    v8::Handle<v8::Object> classObject = classObjectOfPrivateScript(scriptState, className);
    v8::Handle<v8::Value> method = classObject->Get(v8String(scriptState->isolate(), methodName));
    if (method.IsEmpty() || !method->IsFunction()) {
        WTF_LOG_ERROR("Private script error: Target DOM method was not found. (Class name = %s, Method name = %s)\n", className.utf8().data(), methodName.utf8().data());
        RELEASE_ASSERT_NOT_REACHED();
    }
    initializeHolderIfNeeded(scriptState, classObject, holder);
    return V8ScriptRunner::callFunction(v8::Handle<v8::Function>::Cast(method), scriptState->executionContext(), holder, argc, argv, scriptState->isolate());
}

bool PrivateScriptRunner::throwDOMExceptionInPrivateScriptIfNeeded(v8::Isolate* isolate, ExceptionState& exceptionState, v8::Handle<v8::Value> exception)
{
    if (exception.IsEmpty() || !exception->IsObject())
        return false;

    v8::Handle<v8::Object> exceptionObject = v8::Handle<v8::Object>::Cast(exception);
    v8::Handle<v8::Value> name = exceptionObject->Get(v8String(isolate, "name"));
    if (name.IsEmpty() || !name->IsString())
        return false;
    String exceptionName = toCoreString(v8::Handle<v8::String>::Cast(name));
    if (exceptionName == "DOMExceptionInPrivateScript") {
        v8::Handle<v8::Value> message = exceptionObject->Get(v8String(isolate, "message"));
        RELEASE_ASSERT(!message.IsEmpty() && message->IsString());
        v8::Handle<v8::Value> code = exceptionObject->Get(v8String(isolate, "code"));
        RELEASE_ASSERT(!code.IsEmpty() && code->IsInt32());
        exceptionState.throwDOMException(toInt32(code), toCoreString(v8::Handle<v8::String>::Cast(message)));
        exceptionState.throwIfNeeded();
        return true;
    }
    if (exceptionName == "TypeError") {
        v8::Handle<v8::Value> message = exceptionObject->Get(v8String(isolate, "message"));
        RELEASE_ASSERT(!message.IsEmpty() && message->IsString());
        exceptionState.throwDOMException(TypeError, toCoreString(v8::Handle<v8::String>::Cast(message)));
        exceptionState.throwIfNeeded();
        return true;
    }
    if (exceptionName == "RangeError") {
        v8::Handle<v8::Value> message = exceptionObject->Get(v8String(isolate, "message"));
        RELEASE_ASSERT(!message.IsEmpty() && message->IsString());
        exceptionState.throwDOMException(RangeError, toCoreString(v8::Handle<v8::String>::Cast(message)));
        exceptionState.throwIfNeeded();
        return true;
    }
    // FIXME: Support other JavaScript errors such as SecurityError.
    return false;
}

} // namespace blink

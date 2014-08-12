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

#define LOG_ERROR_ALWAYS(...) WTFReportError(__FILE__, __LINE__, WTF_PRETTY_FUNCTION, __VA_ARGS__)

static v8::Handle<v8::Value> compileAndRunInternalScript(v8::Isolate* isolate, String className, const unsigned char* source, size_t size)
{
    v8::TryCatch block;
    String sourceString(reinterpret_cast<const char*>(source), size);
    v8::Handle<v8::Value> result = V8ScriptRunner::compileAndRunInternalScript(v8String(isolate, sourceString), isolate);
    if (block.HasCaught()) {
        LOG_ERROR_ALWAYS("Private script error: Compile failed. (Class name = %s)\n", className.utf8().data());
        if (!block.Message().IsEmpty())
            LOG_ERROR_ALWAYS("%s\n", toCoreString(block.Message()->Get()).utf8().data());
        RELEASE_ASSERT_NOT_REACHED();
    }
    return result;
}

// FIXME: If we have X.js, XPartial-1.js and XPartial-2.js, currently all of the JS files
// are compiled when any of the JS files is requested. Ideally we should avoid compiling
// unrelated JS files. For example, if a method in XPartial-1.js is requested, we just
// need to compile X.js and XPartial-1.js, and don't need to compile XPartial-2.js.
static void compilePrivateScript(v8::Isolate* isolate, String className)
{
    int compiledScriptCount = 0;
    // |kPrivateScriptSourcesForTesting| is defined in V8PrivateScriptSources.h, which is auto-generated
    // by make_private_script_source.py.
#ifndef NDEBUG
    for (size_t index = 0; index < WTF_ARRAY_LENGTH(kPrivateScriptSourcesForTesting); index++) {
        if (className == kPrivateScriptSourcesForTesting[index].dependencyClassName) {
            compileAndRunInternalScript(isolate, className, kPrivateScriptSourcesForTesting[index].source, kPrivateScriptSourcesForTesting[index].size);
            compiledScriptCount++;
        }
    }
#endif

    // |kPrivateScriptSources| is defined in V8PrivateScriptSources.h, which is auto-generated
    // by make_private_script_source.py.
    for (size_t index = 0; index < WTF_ARRAY_LENGTH(kPrivateScriptSources); index++) {
        if (className == kPrivateScriptSources[index].dependencyClassName) {
            compileAndRunInternalScript(isolate, className, kPrivateScriptSources[index].source, kPrivateScriptSources[index].size);
            compiledScriptCount++;
        }
    }

    if (!compiledScriptCount) {
        LOG_ERROR_ALWAYS("Private script error: Target source code was not found. (Class name = %s)\n", className.utf8().data());
        RELEASE_ASSERT_NOT_REACHED();
    }
}

static v8::Handle<v8::Value> compilePrivateScriptRunner(v8::Isolate* isolate)
{
    const String className = "PrivateScriptRunner";
    size_t index;
    // |kPrivateScriptSources| is defined in V8PrivateScriptSources.h, which is auto-generated
    // by make_private_script_source.py.
    for (index = 0; index < WTF_ARRAY_LENGTH(kPrivateScriptSources); index++) {
        if (className == kPrivateScriptSources[index].className)
            break;
    }
    if (index == WTF_ARRAY_LENGTH(kPrivateScriptSources)) {
        LOG_ERROR_ALWAYS("Private script error: Target source code was not found. (Class name = %s)\n", className.utf8().data());
        RELEASE_ASSERT_NOT_REACHED();
    }
    return compileAndRunInternalScript(isolate, className, kPrivateScriptSources[index].source, kPrivateScriptSources[index].size);
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
            installedClasses = compilePrivateScriptRunner(isolate);
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
                LOG_ERROR_ALWAYS("Private script error: Object constructor threw an exception.\n");
                if (!block.Message().IsEmpty())
                    LOG_ERROR_ALWAYS("%s\n", toCoreString(block.Message()->Get()).utf8().data());
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
        LOG_ERROR_ALWAYS("Private script error: Target DOM attribute getter was not found. (Class name = %s, Attribute name = %s)\n", className.utf8().data(), attributeName.utf8().data());
        RELEASE_ASSERT_NOT_REACHED();
    }
    v8::Handle<v8::Value> getter = v8::Handle<v8::Object>::Cast(descriptor)->Get(v8String(scriptState->isolate(), "get"));
    if (getter.IsEmpty() || !getter->IsFunction()) {
        LOG_ERROR_ALWAYS("Private script error: Target DOM attribute getter was not found. (Class name = %s, Attribute name = %s)\n", className.utf8().data(), attributeName.utf8().data());
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
        LOG_ERROR_ALWAYS("Private script error: Target DOM attribute setter was not found. (Class name = %s, Attribute name = %s)\n", className.utf8().data(), attributeName.utf8().data());
        RELEASE_ASSERT_NOT_REACHED();
    }
    v8::Handle<v8::Value> setter = v8::Handle<v8::Object>::Cast(descriptor)->Get(v8String(scriptState->isolate(), "set"));
    if (setter.IsEmpty() || !setter->IsFunction()) {
        LOG_ERROR_ALWAYS("Private script error: Target DOM attribute setter was not found. (Class name = %s, Attribute name = %s)\n", className.utf8().data(), attributeName.utf8().data());
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
        LOG_ERROR_ALWAYS("Private script error: Target DOM method was not found. (Class name = %s, Method name = %s)\n", className.utf8().data(), methodName.utf8().data());
        RELEASE_ASSERT_NOT_REACHED();
    }
    initializeHolderIfNeeded(scriptState, classObject, holder);
    return V8ScriptRunner::callFunction(v8::Handle<v8::Function>::Cast(method), scriptState->executionContext(), holder, argc, argv, scriptState->isolate());
}

static void dumpJSError(String exceptionName, String message)
{
    // FIXME: Set a ScriptOrigin of the private script and print a more informative message.
#ifndef NDEBUG
    fprintf(stderr, "Private script throws an exception: %s\n", exceptionName.utf8().data());
    if (!message.isEmpty())
        fprintf(stderr, "%s\n", message.utf8().data());
#endif
}

bool PrivateScriptRunner::rethrowExceptionInPrivateScript(v8::Isolate* isolate, ExceptionState& exceptionState, v8::TryCatch& block)
{
    v8::Handle<v8::Value> exception = block.Exception();
    if (exception.IsEmpty() || !exception->IsObject())
        return false;

    v8::Handle<v8::Object> exceptionObject = v8::Handle<v8::Object>::Cast(exception);
    v8::Handle<v8::Value> name = exceptionObject->Get(v8String(isolate, "name"));
    if (name.IsEmpty() || !name->IsString())
        return false;
    String exceptionName = toCoreString(v8::Handle<v8::String>::Cast(name));
    if (exceptionName == "DOMExceptionInPrivateScript") {
        v8::Handle<v8::Value> v8Message = exceptionObject->Get(v8String(isolate, "message"));
        RELEASE_ASSERT(!v8Message.IsEmpty() && v8Message->IsString());
        v8::Handle<v8::Value> code = exceptionObject->Get(v8String(isolate, "code"));
        RELEASE_ASSERT(!code.IsEmpty() && code->IsInt32());
        exceptionState.throwDOMException(toInt32(code), toCoreString(v8::Handle<v8::String>::Cast(v8Message)));
        exceptionState.throwIfNeeded();
        return true;
    }
    if (exceptionName == "Error") {
        v8::Handle<v8::Value> v8Message = exceptionObject->Get(v8String(isolate, "message"));
        RELEASE_ASSERT(!v8Message.IsEmpty() && v8Message->IsString());
        String message = toCoreString(v8::Handle<v8::String>::Cast(v8Message));
        exceptionState.throwDOMException(V8GeneralError, message);
        exceptionState.throwIfNeeded();
        dumpJSError(exceptionName, message);
        return true;
    }
    if (exceptionName == "TypeError") {
        v8::Handle<v8::Value> v8Message = exceptionObject->Get(v8String(isolate, "message"));
        RELEASE_ASSERT(!v8Message.IsEmpty() && v8Message->IsString());
        String message = toCoreString(v8::Handle<v8::String>::Cast(v8Message));
        exceptionState.throwDOMException(V8TypeError, message);
        exceptionState.throwIfNeeded();
        dumpJSError(exceptionName, message);
        return true;
    }
    if (exceptionName == "RangeError") {
        v8::Handle<v8::Value> v8Message = exceptionObject->Get(v8String(isolate, "message"));
        RELEASE_ASSERT(!v8Message.IsEmpty() && v8Message->IsString());
        String message = toCoreString(v8::Handle<v8::String>::Cast(v8Message));
        exceptionState.throwDOMException(V8RangeError, message);
        exceptionState.throwIfNeeded();
        dumpJSError(exceptionName, message);
        return true;
    }
    if (exceptionName == "SyntaxError") {
        v8::Handle<v8::Value> v8Message = exceptionObject->Get(v8String(isolate, "message"));
        RELEASE_ASSERT(!v8Message.IsEmpty() && v8Message->IsString());
        String message = toCoreString(v8::Handle<v8::String>::Cast(v8Message));
        exceptionState.throwDOMException(V8SyntaxError, message);
        exceptionState.throwIfNeeded();
        dumpJSError(exceptionName, message);
        return true;
    }
    if (exceptionName == "ReferenceError") {
        v8::Handle<v8::Value> v8Message = exceptionObject->Get(v8String(isolate, "message"));
        RELEASE_ASSERT(!v8Message.IsEmpty() && v8Message->IsString());
        String message = toCoreString(v8::Handle<v8::String>::Cast(v8Message));
        exceptionState.throwDOMException(V8ReferenceError, message);
        exceptionState.throwIfNeeded();
        dumpJSError(exceptionName, message);
        return true;
    }
    return false;
}

} // namespace blink

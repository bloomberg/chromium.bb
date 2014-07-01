// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "bindings/core/v8/PrivateScriptRunner.h"

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

static v8::Handle<v8::Value> propertyValue(ScriptState* scriptState, String className, String propertyName)
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
    return v8::Handle<v8::Object>::Cast(compiledClass)->Get(v8String(isolate, propertyName));
}

v8::Handle<v8::Value> PrivateScriptRunner::runDOMMethod(ScriptState* scriptState, String className, String operationName, v8::Handle<v8::Value> holder, int argc, v8::Handle<v8::Value> argv[])
{
    v8::Handle<v8::Value> operation = propertyValue(scriptState, className, operationName);
    RELEASE_ASSERT(!operation.IsEmpty());
    RELEASE_ASSERT(operation->IsFunction());
    return V8ScriptRunner::callFunction(v8::Handle<v8::Function>::Cast(operation), scriptState->executionContext(), holder, argc, argv, scriptState->isolate());
}

} // namespace WebCore

// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "bindings/v8/PrivateScriptRunner.h"

#include "bindings/v8/V8Binding.h"
#include "bindings/v8/V8PerContextData.h"
#include "bindings/v8/V8ScriptRunner.h"
#include "core/PrivateScriptSources.h"

namespace WebCore {

static v8::Handle<v8::Value> compilePrivateScript(v8::Isolate* isolate, String className)
{
    size_t index;
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

v8::Handle<v8::Value> PrivateScriptRunner::run(ScriptState* scriptState, String className, String functionName, v8::Handle<v8::Value> receiver, int argc, v8::Handle<v8::Value> argv[])
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

    v8::Handle<v8::Value> function = v8::Handle<v8::Object>::Cast(compiledClass)->Get(v8String(isolate, functionName));
    RELEASE_ASSERT(!function.IsEmpty());
    RELEASE_ASSERT(function->IsFunction());

    return V8ScriptRunner::callFunction(v8::Handle<v8::Function>::Cast(function), scriptState->executionContext(), receiver, argc, argv, isolate);
}

} // namespace WebCore

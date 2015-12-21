// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/inspector/DebuggerScript.h"

#include "bindings/core/v8/V8Binding.h"
#include "bindings/core/v8/V8ScriptRunner.h"
#include "public/platform/Platform.h"
#include "public/platform/WebData.h"

namespace blink {

v8::Local<v8::Object> compileDebuggerScript(v8::Isolate* isolate)
{
    const WebData& debuggerScriptSourceResource = Platform::current()->loadResource("DebuggerScriptSource.js");
    v8::Local<v8::String> source = v8String(isolate, String(debuggerScriptSourceResource.data(), debuggerScriptSourceResource.size()));
    v8::Local<v8::Value> value;
    if (!V8ScriptRunner::compileAndRunInternalScript(source, isolate).ToLocal(&value))
        return v8::Local<v8::Object>();
    ASSERT(value->IsObject());
    return value.As<v8::Object>();
}

}

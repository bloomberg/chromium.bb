// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8DebuggerClient_h
#define V8DebuggerClient_h

#include "platform/PlatformExport.h"
#include "platform/v8_inspector/public/V8EventListenerInfo.h"

#include <v8.h>

namespace blink {

class PLATFORM_EXPORT V8DebuggerClient {
    USING_FAST_MALLOC(V8DebuggerClient);
public:
    virtual ~V8DebuggerClient() { }
    virtual void runMessageLoopOnPause(int contextGroupId) = 0;
    virtual void quitMessageLoopOnPause() = 0;
    virtual void muteWarningsAndDeprecations() = 0;
    virtual void unmuteWarningsAndDeprecations() = 0;
    virtual void eventListeners(v8::Local<v8::Value>, V8EventListenerInfoMap&) = 0;
    virtual bool callingContextCanAccessContext(v8::Local<v8::Context> calling, v8::Local<v8::Context> target) = 0;

    virtual v8::MaybeLocal<v8::Object> instantiateObject(v8::Local<v8::Function>) = 0;
    virtual v8::MaybeLocal<v8::Value> runCompiledScript(v8::Local<v8::Context>, v8::Local<v8::Script>) = 0;
    virtual v8::MaybeLocal<v8::Value> compileAndRunInternalScript(v8::Local<v8::String>) = 0;
    virtual v8::MaybeLocal<v8::Value> callFunction(v8::Local<v8::Function>, v8::Local<v8::Context>, v8::Local<v8::Value> receiver, int argc, v8::Local<v8::Value> info[]) = 0;
    virtual v8::MaybeLocal<v8::Value> callInternalFunction(v8::Local<v8::Function>, v8::Local<v8::Value> receiver, int argc, v8::Local<v8::Value> info[]) = 0;

    virtual String valueSubtype(v8::Local<v8::Value>) = 0;
    virtual bool formatAccessorsAsProperties(v8::Local<v8::Value>) = 0;
    virtual bool hasRecursionLevel() = 0;
};

} // namespace blink


#endif // V8DebuggerClient_h

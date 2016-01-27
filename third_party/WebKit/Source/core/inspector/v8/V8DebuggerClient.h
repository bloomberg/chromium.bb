// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8DebuggerClient_h
#define V8DebuggerClient_h

#include "core/CoreExport.h"
#include "core/inspector/v8/EventListenerInfo.h"

#include <v8.h>

namespace blink {

class CORE_EXPORT V8DebuggerClient {
    USING_FAST_MALLOC(V8DebuggerClient);
public:
    virtual ~V8DebuggerClient() { }
    virtual v8::Local<v8::Object> compileDebuggerScript() = 0;
    virtual void runMessageLoopOnPause(int contextGroupId) = 0;
    virtual void quitMessageLoopOnPause() = 0;
    virtual void eventListeners(v8::Local<v8::Value>, EventListenerInfoMap&) = 0;
    virtual bool callingContextCanAccessContext(v8::Local<v8::Context>) = 0;
    virtual v8::MaybeLocal<v8::Value> callFunction(v8::Local<v8::Function>, v8::Local<v8::Context>, v8::Local<v8::Value> receiver, int argc, v8::Local<v8::Value> info[]) = 0;
};

} // namespace blink


#endif // V8DebuggerClient_h

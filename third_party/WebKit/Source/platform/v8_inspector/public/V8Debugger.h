// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8Debugger_h
#define V8Debugger_h

#include "platform/PlatformExport.h"
#include "platform/inspector_protocol/Frontend.h"
#include "wtf/Forward.h"
#include "wtf/PassOwnPtr.h"

#include <v8.h>

namespace blink {

class JSONObject;
class V8DebuggerClient;
class V8StackTrace;

class PLATFORM_EXPORT V8Debugger {
    USING_FAST_MALLOC(V8Debugger);
public:
    template <typename T>
    class Agent {
    public:
        virtual void setInspectorState(PassRefPtr<JSONObject>) = 0;
        virtual void setFrontend(T*) = 0;
        virtual void clearFrontend() = 0;
        virtual void restore() = 0;
    };

    static PassOwnPtr<V8Debugger> create(v8::Isolate*, V8DebuggerClient*);
    virtual ~V8Debugger() { }

    // Each v8::Context is a part of a group. The group id is used to find approapriate
    // V8DebuggerAgent to notify about events in the context.
    // |contextGroupId| must be non-0.
    static void setContextDebugData(v8::Local<v8::Context>, const String& type, int contextGroupId);
    static int contextId(v8::Local<v8::Context>);

    static v8::Local<v8::Symbol> commandLineAPISymbol(v8::Isolate*);
    static bool isCommandLineAPIMethod(const AtomicString& name);

    virtual PassOwnPtr<V8StackTrace> createStackTrace(v8::Local<v8::StackTrace>, size_t maxStackSize) = 0;
    virtual PassOwnPtr<V8StackTrace> captureStackTrace(size_t maxStackSize) = 0;
};

} // namespace blink


#endif // V8Debugger_h

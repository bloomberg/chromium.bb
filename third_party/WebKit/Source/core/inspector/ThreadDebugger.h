// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ThreadDebugger_h
#define ThreadDebugger_h

#include "core/CoreExport.h"
#include "core/inspector/v8/V8Debugger.h"
#include "core/inspector/v8/V8DebuggerClient.h"
#include "wtf/Forward.h"

#include <v8.h>

namespace blink {

class WorkerThread;

class CORE_EXPORT ThreadDebugger : public V8DebuggerClient {
    WTF_MAKE_NONCOPYABLE(ThreadDebugger);
public:
    explicit ThreadDebugger(v8::Isolate*);
    ~ThreadDebugger() override;

    // V8DebuggerClient implementation.
    void eventListeners(v8::Local<v8::Value>, EventListenerInfoMap&) override;
    v8::MaybeLocal<v8::Object> instantiateObject(v8::Local<v8::Function>) override;
    v8::MaybeLocal<v8::Value> runCompiledScript(v8::Local<v8::Context>, v8::Local<v8::Script>) override;
    v8::MaybeLocal<v8::Value> compileAndRunInternalScript(v8::Local<v8::String>) override;
    v8::MaybeLocal<v8::Value> callFunction(v8::Local<v8::Function>, v8::Local<v8::Context>, v8::Local<v8::Value> receiver, int argc, v8::Local<v8::Value> info[]) override;
    v8::MaybeLocal<v8::Value> callInternalFunction(v8::Local<v8::Function>, v8::Local<v8::Value> receiver, int argc, v8::Local<v8::Value> info[]) override;
    String valueSubtype(v8::Local<v8::Value>) override;
    bool formatAccessorsAsProperties(v8::Local<v8::Value>) override;

    V8Debugger* debugger() const { return m_debugger.get(); }

protected:
    v8::Isolate* m_isolate;
    OwnPtr<V8Debugger> m_debugger;
};

} // namespace blink

#endif // ThreadDebugger_h

// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ThreadDebugger_h
#define ThreadDebugger_h

#include "core/CoreExport.h"
#include "platform/v8_inspector/public/V8Debugger.h"
#include "platform/v8_inspector/public/V8DebuggerClient.h"
#include "wtf/Forward.h"

#include <v8.h>

namespace blink {

class ConsoleMessage;
class WorkerThread;

class CORE_EXPORT ThreadDebugger : public V8DebuggerClient {
    WTF_MAKE_NONCOPYABLE(ThreadDebugger);
public:
    explicit ThreadDebugger(v8::Isolate*);
    ~ThreadDebugger() override;

    static void willExecuteScript(v8::Isolate*, int scriptId);
    static void didExecuteScript(v8::Isolate*);

    // V8DebuggerClient implementation.
    void eventListeners(v8::Local<v8::Value>, V8EventListenerInfoList&) override;
    String16 valueSubtype(v8::Local<v8::Value>) override;
    bool formatAccessorsAsProperties(v8::Local<v8::Value>) override;
    bool isExecutionAllowed() override;
    double currentTimeMS() override;
    void reportMessageToConsole(v8::Local<v8::Context>, MessageType, MessageLevel, const String16& message, const v8::FunctionCallbackInfo<v8::Value>* arguments, unsigned skipArgumentCount, int maxStackSize) final;
    void consoleTime(const String16& title) override;
    void consoleTimeEnd(const String16& title) override;
    void consoleTimeStamp(const String16& title) override;

    V8Debugger* debugger() const { return m_debugger.get(); }
    virtual bool isWorker() { return true; }
protected:
    virtual void reportMessageToConsole(v8::Local<v8::Context>, ConsoleMessage*) = 0;

    v8::Isolate* m_isolate;
    OwnPtr<V8Debugger> m_debugger;
};

} // namespace blink

#endif // ThreadDebugger_h

// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SimpleInspector_h
#define SimpleInspector_h

#include "platform/inspector_protocol/Platform.h"
#include "platform/v8_inspector/public/V8InspectorClient.h"
#include "platform/v8_inspector/public/V8InspectorSession.h"

#include <v8.h>

namespace blink {

namespace protocol {
class Dispatcher;
class Frontend;
class FrontendChannel;
}

class V8Inspector;
class V8HeapProfilerAgent;
class V8ProfilerAgent;

class SimpleInspector : public V8InspectorClient {
public:
    SimpleInspector(v8::Isolate*, v8::Local<v8::Context>);
    ~SimpleInspector();

    // Transport interface.
    void connectFrontend(protocol::FrontendChannel*);
    void disconnectFrontend();
    void dispatchMessageFromFrontend(const String16& message);
    void notifyContextDestroyed();

private:
    String16 valueSubtype(v8::Local<v8::Value>) override;
    bool formatAccessorsAsProperties(v8::Local<v8::Value>) override;
    void muteWarningsAndDeprecations(int) override { }
    void unmuteWarningsAndDeprecations(int) override { }
    double currentTimeMS() override { return 0; };

    bool isExecutionAllowed() override;
    v8::Local<v8::Context> ensureDefaultContextInGroup(int contextGroupId) override;
    void beginEnsureAllContextsInGroup(int contextGroupId) override { }
    void endEnsureAllContextsInGroup(int contextGroupId) override { }
    void beginUserGesture() override { }
    void endUserGesture() override { }
    bool isInspectableHeapObject(v8::Local<v8::Object>) override { return true; }
    void consoleTime(const String16& title) override { }
    void consoleTimeEnd(const String16& title) override { }
    void consoleTimeStamp(const String16& title) override { }
    void consoleAPIMessage(int contextGroupId, V8ConsoleAPIType, const String16& message, const String16& url, unsigned lineNumber, unsigned columnNumber, V8StackTrace*) override { }
    v8::MaybeLocal<v8::Value> memoryInfo(v8::Isolate*, v8::Local<v8::Context>) override
    {
        return v8::MaybeLocal<v8::Value>();
    }
    void installAdditionalCommandLineAPI(v8::Local<v8::Context>, v8::Local<v8::Object>) override { }
    void startRepeatingTimer(double, TimerCallback, void* data) override { }
    void cancelTimer(void* data) override { }
    bool canExecuteScripts(int) override { return true; }
    void resumeStartup(int) override { }

    std::unique_ptr<V8Inspector> m_inspector;
    std::unique_ptr<V8InspectorSession> m_session;
    String16 m_state;
    v8::Local<v8::Context> m_context;
};

}

#endif // SimpleInspector_h

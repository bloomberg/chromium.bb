// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8InspectorSessionImpl_h
#define V8InspectorSessionImpl_h

#include "platform/inspector_protocol/Allocator.h"
#include "platform/inspector_protocol/Collections.h"
#include "platform/inspector_protocol/String16.h"
#include "platform/inspector_protocol/TypeBuilder.h"
#include "platform/v8_inspector/public/V8InspectorSession.h"
#include "platform/v8_inspector/public/V8InspectorSessionClient.h"
#include "platform/v8_inspector/public/V8RuntimeAgent.h"
#include "wtf/PassOwnPtr.h"

#include <v8.h>

namespace blink {

class InjectedScript;
class InjectedScriptHost;
class RemoteObjectIdBase;
class V8DebuggerAgentImpl;
class V8DebuggerImpl;
class V8HeapProfilerAgentImpl;
class V8ProfilerAgentImpl;
class V8RuntimeAgentImpl;

class V8InspectorSessionImpl : public V8InspectorSession {
    PROTOCOL_DISALLOW_COPY(V8InspectorSessionImpl);
public:
    static PassOwnPtr<V8InspectorSessionImpl> create(V8DebuggerImpl*, int contextGroupId);
    ~V8InspectorSessionImpl();

    V8DebuggerImpl* debugger() const { return m_debugger; }
    V8InspectorSessionClient* client() const { return m_client; }
    V8DebuggerAgentImpl* debuggerAgentImpl() { return m_debuggerAgent.get(); }
    V8ProfilerAgentImpl* profilerAgentImpl() { return m_profilerAgent.get(); }
    V8RuntimeAgentImpl* runtimeAgentImpl() { return m_runtimeAgent.get(); }
    int contextGroupId() const { return m_contextGroupId; }

    InjectedScript* findInjectedScript(ErrorString*, int contextId);
    InjectedScript* findInjectedScript(ErrorString*, RemoteObjectIdBase*);
    void reset();
    void discardInjectedScripts();
    void reportAllContexts(V8RuntimeAgentImpl*);
    void addInspectedObject(PassOwnPtr<V8RuntimeAgent::Inspectable>);
    void releaseObjectGroup(const String16& objectGroup);
    void setCustomObjectFormatterEnabled(bool);
    void changeInstrumentationCounter(int delta);

    // V8InspectorSession implementation.
    void setClient(V8InspectorSessionClient*) override;
    V8DebuggerAgent* debuggerAgent() override;
    V8HeapProfilerAgent* heapProfilerAgent() override;
    V8ProfilerAgent* profilerAgent() override;
    V8RuntimeAgent* runtimeAgent() override;

    void setClearConsoleCallback(PassOwnPtr<V8RuntimeAgent::ClearConsoleCallback> callback) { m_clearConsoleCallback = callback; }
    V8RuntimeAgent::ClearConsoleCallback* clearConsoleCallback() { return m_clearConsoleCallback.get(); }

private:
    V8InspectorSessionImpl(V8DebuggerImpl*, int contextGroupId);

    int m_contextGroupId;
    V8DebuggerImpl* m_debugger;
    V8InspectorSessionClient* m_client;
    OwnPtr<InjectedScriptHost> m_injectedScriptHost;
    bool m_customObjectFormatterEnabled;
    int m_instrumentationCounter;

    OwnPtr<V8RuntimeAgentImpl> m_runtimeAgent;
    OwnPtr<V8DebuggerAgentImpl> m_debuggerAgent;
    OwnPtr<V8HeapProfilerAgentImpl> m_heapProfilerAgent;
    OwnPtr<V8ProfilerAgentImpl> m_profilerAgent;

    OwnPtr<V8RuntimeAgent::ClearConsoleCallback> m_clearConsoleCallback;
};

} // namespace blink

#endif

// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8InspectorConnectionImpl_h
#define V8InspectorConnectionImpl_h

#include "platform/inspector_protocol/Allocator.h"
#include "platform/inspector_protocol/Collections.h"
#include "platform/inspector_protocol/String16.h"
#include "platform/inspector_protocol/TypeBuilder.h"
#include "platform/v8_inspector/public/V8RuntimeAgent.h"
#include "wtf/PassOwnPtr.h"

#include <v8.h>

namespace blink {

class InjectedScript;
class InjectedScriptHost;
class InjectedScriptNative;
class InspectedContext;
class RemoteObjectIdBase;
class V8ContextInfo;
class V8DebuggerAgentImpl;
class V8DebuggerImpl;
class V8RuntimeAgentImpl;

class V8InspectorConnectionImpl {
    PROTOCOL_DISALLOW_COPY(V8InspectorConnectionImpl);
public:
    static PassOwnPtr<V8InspectorConnectionImpl> create(V8DebuggerImpl*, int contextGroupId);
    ~V8InspectorConnectionImpl();

    V8DebuggerImpl* debugger() const { return m_debugger; }
    int contextGroupId() const { return m_contextGroupId; }

    InjectedScript* findInjectedScript(ErrorString*, int contextId);
    InjectedScript* findInjectedScript(ErrorString*, RemoteObjectIdBase*);
    void resetInjectedScripts();
    void reportAllContexts(V8RuntimeAgentImpl*);
    void addInspectedObject(PassOwnPtr<V8RuntimeAgent::Inspectable>);
    void releaseObjectGroup(const String16& objectGroup);
    void setCustomObjectFormatterEnabled(bool);

    // TODO(dgozman): remove these once runtime and debugger agent have the same lifetime.
    void setDebuggerAgent(V8DebuggerAgentImpl* agent) { m_debuggerAgent = agent; }
    V8DebuggerAgentImpl* debuggerAgent() { return m_debuggerAgent; }
    void setClearConsoleCallback(PassOwnPtr<V8RuntimeAgent::ClearConsoleCallback> callback) { m_clearConsoleCallback = callback; }
    V8RuntimeAgent::ClearConsoleCallback* clearConsoleCallback() { return m_clearConsoleCallback.get(); }
    void setInspectObjectCallback(PassOwnPtr<V8RuntimeAgent::InspectCallback> callback) { m_inspectCallback = callback; }
    V8RuntimeAgent::InspectCallback* inspectCallback() { return m_inspectCallback.get(); }

private:
    friend class InspectedContext;
    V8InspectorConnectionImpl(V8DebuggerImpl*, int contextGroupId);
    int m_contextGroupId;
    V8DebuggerImpl* m_debugger;
    OwnPtr<InjectedScriptHost> m_injectedScriptHost;
    bool m_customObjectFormatterEnabled;
    V8DebuggerAgentImpl* m_debuggerAgent;
    OwnPtr<V8RuntimeAgent::InspectCallback> m_inspectCallback;
    OwnPtr<V8RuntimeAgent::ClearConsoleCallback> m_clearConsoleCallback;
};

} // namespace blink

#endif

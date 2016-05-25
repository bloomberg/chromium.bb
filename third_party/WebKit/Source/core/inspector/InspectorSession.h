// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef InspectorSession_h
#define InspectorSession_h

#include "core/CoreExport.h"
#include "platform/heap/Handle.h"
#include "platform/inspector_protocol/Dispatcher.h"
#include "platform/inspector_protocol/Frontend.h"
#include "platform/inspector_protocol/FrontendChannel.h"
#include "platform/inspector_protocol/Values.h"
#include "platform/v8_inspector/public/V8InspectorSessionClient.h"
#include "wtf/Forward.h"
#include "wtf/PassOwnPtr.h"
#include "wtf/Vector.h"
#include "wtf/text/WTFString.h"

namespace blink {

class ExecutionContext;
class InspectedFrames;
class InspectorAgent;
class InstrumentingAgents;
class LocalFrame;
class V8Debugger;
class V8InspectorSession;

class CORE_EXPORT InspectorSession
    : public GarbageCollectedFinalized<InspectorSession>
    , public V8InspectorSessionClient {
    WTF_MAKE_NONCOPYABLE(InspectorSession);
public:
    class Client {
    public:
        virtual void sendProtocolMessage(int sessionId, int callId, const String& response, const String& state) = 0;
        virtual void resumeStartup() { }
        virtual void profilingStarted() { }
        virtual void profilingStopped() { }
        virtual ~Client() {}
    };

    InspectorSession(Client*, InspectedFrames*, InstrumentingAgents*, int sessionId, bool autoFlush, V8Debugger*, int contextGroupId, const String* savedState);
    ~InspectorSession() override;
    int sessionId() { return m_sessionId; }
    V8InspectorSession* v8Session() { return m_v8Session.get(); }

    void append(InspectorAgent*);
    void restore();
    void dispose();
    void didCommitLoadForLocalFrame(LocalFrame*);
    void dispatchProtocolMessage(const String& method, const String& message);
    void flushProtocolNotifications() override;

    // Instrumentation methods marked by [V8]
    void scriptExecutionBlockedByCSP(const String& directiveText);
    void asyncTaskScheduled(const String& taskName, void* task);
    void asyncTaskScheduled(const String& taskName, void* task, bool recurring);
    void asyncTaskCanceled(void* task);
    void allAsyncTasksCanceled();
    void asyncTaskStarted(void* task);
    void asyncTaskFinished(void* task);
    void didStartProvisionalLoad(LocalFrame*);
    void didClearDocumentOfWindowObject(LocalFrame*);

    DECLARE_TRACE();

private:
    // protocol::FrontendChannel implementation.
    void sendProtocolResponse(int callId, const protocol::String16& message) override;
    void sendProtocolNotification(const protocol::String16& message) override;

    // V8InspectorSessionClient implementation.
    void startInstrumenting() override;
    void stopInstrumenting() override;
    void resumeStartup() override;
    bool canExecuteScripts() override;
    void profilingStarted() override;
    void profilingStopped() override;

    void forceContextsInAllFrames();
    bool isInstrumenting();

    Client* m_client;
    std::unique_ptr<V8InspectorSession> m_v8Session;
    int m_sessionId;
    bool m_autoFlush;
    bool m_disposed;
    Member<InspectedFrames> m_inspectedFrames;
    Member<InstrumentingAgents> m_instrumentingAgents;
    std::unique_ptr<protocol::Frontend> m_inspectorFrontend;
    std::unique_ptr<protocol::Dispatcher> m_inspectorBackendDispatcher;
    std::unique_ptr<protocol::DictionaryValue> m_state;
    HeapVector<Member<InspectorAgent>> m_agents;
    Vector<protocol::String16> m_notificationQueue;
    String m_lastSentState;
};

} // namespace blink

#endif // !defined(InspectorSession_h)

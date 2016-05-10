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
class V8InspectorSession;

class CORE_EXPORT InspectorSession
    : public GarbageCollectedFinalized<InspectorSession>
    , WTF_NON_EXPORTED_BASE(public protocol::FrontendChannel)
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

    InspectorSession(Client*, InspectedFrames*, InstrumentingAgents*, int sessionId, bool autoFlush);
    int sessionId() { return m_sessionId; }

    void append(InspectorAgent*);
    void attach(V8InspectorSession*, const String* savedState);
    void detach();
    void didCommitLoadForLocalFrame(LocalFrame*);
    void dispatchProtocolMessage(const String& message);
    void flushPendingProtocolNotifications();

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
    void sendProtocolResponse(int sessionId, int callId, PassOwnPtr<protocol::DictionaryValue> message) override;
    void sendProtocolNotification(PassOwnPtr<protocol::DictionaryValue> message) override;
    void flush();

    // V8InspectorSessionClient implementation.
    void startInstrumenting() override;
    void stopInstrumenting() override;
    void resumeStartup() override;
    bool canExecuteScripts() override;
    void profilingStarted() override;
    void profilingStopped() override;

    void forceContextsInAllFrames();
#if ENABLE(ASSERT)
    bool isInstrumenting();
#endif

    Client* m_client;
    V8InspectorSession* m_v8Session;
    int m_sessionId;
    bool m_autoFlush;
    bool m_attached;
    Member<InspectedFrames> m_inspectedFrames;
    Member<InstrumentingAgents> m_instrumentingAgents;
    OwnPtr<protocol::Frontend> m_inspectorFrontend;
    OwnPtr<protocol::Dispatcher> m_inspectorBackendDispatcher;
    OwnPtr<protocol::DictionaryValue> m_state;
    HeapVector<Member<InspectorAgent>> m_agents;
    Vector<OwnPtr<protocol::DictionaryValue>> m_notificationQueue;
    String m_lastSentState;
};

} // namespace blink

#endif // !defined(InspectorSession_h)

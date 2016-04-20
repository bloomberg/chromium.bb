// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef InspectorSession_h
#define InspectorSession_h

#include "core/CoreExport.h"
#include "platform/heap/Handle.h"
#include "platform/inspector_protocol/FrontendChannel.h"
#include "platform/inspector_protocol/Values.h"
#include "wtf/Forward.h"
#include "wtf/PassOwnPtr.h"
#include "wtf/Vector.h"
#include "wtf/text/WTFString.h"

namespace blink {

class InspectorAgent;
class InstrumentingAgents;
class LocalFrame;

namespace protocol {
class Dispatcher;
class Frontend;
}

class CORE_EXPORT InspectorSession
    : public GarbageCollectedFinalized<InspectorSession>
    , WTF_NON_EXPORTED_BASE(public protocol::FrontendChannel) {
    WTF_MAKE_NONCOPYABLE(InspectorSession);
public:
    class Client {
    public:
        virtual void sendProtocolMessage(int sessionId, int callId, const String& response, const String& state) = 0;
        virtual ~Client() {}
    };

    InspectorSession(Client*, int sessionId, InstrumentingAgents*, bool autoFlush);
    int sessionId() { return m_sessionId; }

    void append(InspectorAgent*);
    void attach(const String* savedState);
    void detach();
    void didCommitLoadForLocalFrame(LocalFrame*);
    void dispatchProtocolMessage(const String& message);
    void flushPendingProtocolNotifications();

    DECLARE_TRACE();

private:
    // protocol::FrontendChannel implementation.
    void sendProtocolResponse(int sessionId, int callId, PassOwnPtr<protocol::DictionaryValue> message) override;
    void sendProtocolNotification(PassOwnPtr<protocol::DictionaryValue> message) override;
    void flush();

    Client* m_client;
    int m_sessionId;
    bool m_autoFlush;
    bool m_attached;
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

/*
 * Copyright 2014 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef InspectorTracingAgent_h
#define InspectorTracingAgent_h

#include "core/InspectorFrontend.h"
#include "core/inspector/InspectorBaseAgent.h"
#include "wtf/PassOwnPtr.h"
#include "wtf/text/WTFString.h"

namespace blink {

class InspectorWorkerAgent;
class LocalFrame;

class InspectorTracingAgent final
    : public InspectorBaseAgent<InspectorTracingAgent, InspectorFrontend::Tracing>
    , public InspectorBackendDispatcher::TracingCommandHandler {
    WTF_MAKE_NONCOPYABLE(InspectorTracingAgent);
public:
    class Client {
    public:
        virtual ~Client() { }

        virtual void enableTracing(const String& categoryFilter) { }
        virtual void disableTracing() { }
    };

    static PassOwnPtrWillBeRawPtr<InspectorTracingAgent> create(LocalFrame* inspectedFrame, Client* client, InspectorWorkerAgent* workerAgent)
    {
        return adoptPtrWillBeNoop(new InspectorTracingAgent(inspectedFrame, client, workerAgent));
    }

    DECLARE_VIRTUAL_TRACE();

    // Base agent methods.
    void restore() override;
    void disable(ErrorString*) override;

    // Protocol method implementations.
    virtual void start(ErrorString*, const String* categoryFilter, const String*, const double*, PassRefPtrWillBeRawPtr<StartCallback>) override;
    virtual void end(ErrorString*, PassRefPtrWillBeRawPtr<EndCallback>);

    // Methods for other agents to use.
    void setLayerTreeId(int);

private:
    InspectorTracingAgent(LocalFrame*, Client*, InspectorWorkerAgent*);

    void emitMetadataEvents();
    void resetSessionId();
    String sessionId();

    RawPtrWillBeMember<LocalFrame> m_inspectedFrame;
    int m_layerTreeId;
    Client* m_client;
    RawPtrWillBeMember<InspectorWorkerAgent> m_workerAgent;
};

} // namespace blink

#endif // InspectorTracingAgent_h

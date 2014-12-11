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

class InspectorClient;
class InspectorWorkerAgent;
class Page;

class InspectorTracingAgent final
    : public InspectorBaseAgent<InspectorTracingAgent>
    , public InspectorBackendDispatcher::TracingCommandHandler {
    WTF_MAKE_NONCOPYABLE(InspectorTracingAgent);
public:
    static PassOwnPtrWillBeRawPtr<InspectorTracingAgent> create(InspectorClient* client, InspectorWorkerAgent* workerAgent, Page* page)
    {
        return adoptPtrWillBeNoop(new InspectorTracingAgent(client, workerAgent, page));
    }

    void trace(Visitor*) override;

    // Base agent methods.
    virtual void restore() override;
    virtual void setFrontend(InspectorFrontend*) override;
    virtual void clearFrontend() override;

    // Protocol method implementations.
    virtual void start(ErrorString*, const String* categoryFilter, const String*, const double*, PassRefPtrWillBeRawPtr<StartCallback>) override;
    virtual void end(ErrorString*, PassRefPtrWillBeRawPtr<EndCallback>);

    // Methods for other agents to use.
    void setLayerTreeId(int);

private:
    InspectorTracingAgent(InspectorClient*, InspectorWorkerAgent*, Page*);

    void emitMetadataEvents();
    void resetSessionId();
    String sessionId();

    int m_layerTreeId;
    InspectorClient* m_client;
    InspectorFrontend::Tracing* m_frontend;
    RawPtrWillBeMember<InspectorWorkerAgent> m_workerAgent;
    RawPtrWillBeMember<Page> m_page;
};

} // namespace blink

#endif // InspectorTracingAgent_h

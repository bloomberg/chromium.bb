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

class InspectorTracingAgent FINAL
    : public InspectorBaseAgent<InspectorTracingAgent>
    , public InspectorBackendDispatcher::TracingCommandHandler {
    WTF_MAKE_NONCOPYABLE(InspectorTracingAgent);
public:
    static PassOwnPtrWillBeRawPtr<InspectorTracingAgent> create(InspectorClient* client, InspectorWorkerAgent* workerAgent)
    {
        return adoptPtrWillBeNoop(new InspectorTracingAgent(client, workerAgent));
    }

    // Base agent methods.
    virtual void restore() OVERRIDE;
    virtual void setFrontend(InspectorFrontend*) OVERRIDE;

    void consoleTimeline(const String& title);
    void consoleTimelineEnd(const String& title);

    // Protocol method implementations.
    virtual void start(ErrorString*, const String& categoryFilter, const String&, const double*) OVERRIDE;
    virtual void end(ErrorString*);

    // Methods for other agents to use.
    void setLayerTreeId(int);

private:
    InspectorTracingAgent(InspectorClient*, InspectorWorkerAgent*);

    void emitMetadataEvents();
    void innerStart(const String& categoryFilter, bool fromConsole);
    String sessionId();
    void notifyTracingStopped();

    int m_layerTreeId;
    InspectorClient* m_client;
    Vector<String> m_consoleTimelines;
    InspectorFrontend::Tracing* m_frontend;
    InspectorWorkerAgent* m_workerAgent;
};

}

#endif // InspectorTracingAgent_h

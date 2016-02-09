/*
 * Copyright 2014 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef InspectorTracingAgent_h
#define InspectorTracingAgent_h

#include "core/CoreExport.h"
#include "core/InspectorFrontend.h"
#include "core/inspector/InspectorBaseAgent.h"
#include "wtf/PassOwnPtr.h"
#include "wtf/text/WTFString.h"

namespace blink {

class InspectedFrames;
class InspectorWorkerAgent;

class CORE_EXPORT InspectorTracingAgent final
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

    static PassOwnPtrWillBeRawPtr<InspectorTracingAgent> create(Client* client, InspectorWorkerAgent* workerAgent, InspectedFrames* inspectedFrames)
    {
        return adoptPtrWillBeNoop(new InspectorTracingAgent(client, workerAgent, inspectedFrames));
    }

    DECLARE_VIRTUAL_TRACE();

    // Base agent methods.
    void restore() override;
    void disable(ErrorString*) override;

    // Protocol method implementations.
    void start(ErrorString*, const String* categoryFilter, const String*, const double*, const String*, PassRefPtr<StartCallback>) override;
    void end(ErrorString*, PassRefPtr<EndCallback>) override;

    // Methods for other agents to use.
    void setLayerTreeId(int);

private:
    InspectorTracingAgent(Client*, InspectorWorkerAgent*, InspectedFrames*);

    void emitMetadataEvents();
    void resetSessionId();
    String sessionId();

    int m_layerTreeId;
    Client* m_client;
    RawPtrWillBeMember<InspectorWorkerAgent> m_workerAgent;
    RawPtrWillBeMember<InspectedFrames> m_inspectedFrames;
};

} // namespace blink

#endif // InspectorTracingAgent_h

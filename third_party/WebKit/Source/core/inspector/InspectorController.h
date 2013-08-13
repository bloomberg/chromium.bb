/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef InspectorController_h
#define InspectorController_h

#include "core/inspector/InspectorBaseAgent.h"
#include "wtf/Forward.h"
#include "wtf/HashMap.h"
#include "wtf/Noncopyable.h"
#include "wtf/text/WTFString.h"

namespace WebCore {

class DOMWrapperWorld;
class Frame;
class GraphicsContext;
class InjectedScriptManager;
class InspectorBackendDispatcher;
class InspectorClient;
class InspectorFrontend;
class InspectorFrontendChannel;
class InspectorFrontendClient;
class InspectorMemoryAgent;
class InspectorTimelineAgent;
class InspectorOverlay;
class InspectorState;
class InstrumentingAgents;
class IntSize;
class Page;
class PlatformGestureEvent;
class PlatformMouseEvent;
class PlatformTouchEvent;
class PostWorkerNotificationToFrontendTask;
class Node;

struct Highlight;

class InspectorController {
    WTF_MAKE_NONCOPYABLE(InspectorController);
    WTF_MAKE_FAST_ALLOCATED;
public:
    ~InspectorController();

    static PassOwnPtr<InspectorController> create(Page*, InspectorClient*);
    void inspectedPageDestroyed();

    Page* inspectedPage() const;

    void setInspectorFrontendClient(PassOwnPtr<InspectorFrontendClient>);
    void didClearWindowObjectInWorld(Frame*, DOMWrapperWorld*);
    void setInjectedScriptForOrigin(const String& origin, const String& source);

    void dispatchMessageFromFrontend(const String& message);

    bool hasFrontend() const { return m_inspectorFrontend; }
    void connectFrontend(InspectorFrontendChannel*);
    void disconnectFrontend();
    void reconnectFrontend();
    void reuseFrontend(InspectorFrontendChannel*, const String& inspectorStateCookie);
    void setProcessId(long);
    void setLayerTreeId(int);
    void webViewResized(const IntSize&);

    void inspect(Node*);
    void drawHighlight(GraphicsContext&) const;
    void getHighlight(Highlight*) const;
    void hideHighlight();
    Node* highlightedNode() const;

    bool handleGestureEvent(Frame*, const PlatformGestureEvent&);
    bool handleMouseEvent(Frame*, const PlatformMouseEvent&);
    bool handleTouchEvent(Frame*, const PlatformTouchEvent&);

    bool isUnderTest();
    void evaluateForTestInFrontend(long callId, const String& script);

    void resume();

    void setResourcesDataSizeLimitsFromInternals(int maximumResourcesContentSize, int maximumSingleResourceContentSize);

    InspectorClient* inspectorClient() const { return m_inspectorClient; }

    void willProcessTask();
    void didProcessTask();

    void didBeginFrame();
    void didCancelFrame();
    void willComposite();
    void didComposite();

private:
    InspectorController(Page*, InspectorClient*);

    friend class PostWorkerNotificationToFrontendTask;
    friend InstrumentingAgents* instrumentationForPage(Page*);

    RefPtr<InstrumentingAgents> m_instrumentingAgents;
    OwnPtr<InjectedScriptManager> m_injectedScriptManager;
    OwnPtr<InspectorCompositeState> m_state;
    OwnPtr<InspectorOverlay> m_overlay;

    InspectorMemoryAgent* m_memoryAgent;
    InspectorTimelineAgent* m_timelineAgent;

    RefPtr<InspectorBackendDispatcher> m_inspectorBackendDispatcher;
    OwnPtr<InspectorFrontendClient> m_inspectorFrontendClient;
    OwnPtr<InspectorFrontend> m_inspectorFrontend;
    Page* m_page;
    InspectorClient* m_inspectorClient;
    InspectorAgentRegistry m_agents;
    bool m_isUnderTest;
};

}


#endif // !defined(InspectorController_h)

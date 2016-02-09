/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
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

#ifndef WebDevToolsAgentImpl_h
#define WebDevToolsAgentImpl_h

#include "core/inspector/InspectorFrontendChannel.h"
#include "core/inspector/InspectorPageAgent.h"
#include "core/inspector/InspectorRuntimeAgent.h"
#include "core/inspector/InspectorTracingAgent.h"
#include "platform/heap/Handle.h"
#include "public/platform/WebSize.h"
#include "public/platform/WebThread.h"
#include "public/web/WebDevToolsAgent.h"
#include "web/InspectorEmulationAgent.h"
#include "wtf/Forward.h"
#include "wtf/OwnPtr.h"
#include "wtf/Vector.h"

namespace blink {

class DebuggerTask;
class GraphicsLayer;
class InspectedFrames;
class InspectorInspectorAgent;
class InspectorOverlay;
class InspectorResourceContentLoader;
class LocalFrame;
class Page;
class PageConsoleAgent;
class PlatformGestureEvent;
class PlatformKeyboardEvent;
class PlatformMouseEvent;
class PlatformTouchEvent;
class WebDevToolsAgentClient;
class WebFrameWidgetImpl;
class WebInputEvent;
class WebLayerTreeView;
class WebLocalFrameImpl;
class WebString;
class WebViewImpl;

class WebDevToolsAgentImpl final
    : public NoBaseWillBeGarbageCollectedFinalized<WebDevToolsAgentImpl>
    , public WebDevToolsAgent
    , public InspectorEmulationAgent::Client
    , public InspectorTracingAgent::Client
    , public InspectorPageAgent::Client
    , public InspectorRuntimeAgent::Client
    , public InspectorFrontendChannel
    , private WebThread::TaskObserver {
public:
    static PassOwnPtrWillBeRawPtr<WebDevToolsAgentImpl> create(WebLocalFrameImpl*, WebDevToolsAgentClient*);
    ~WebDevToolsAgentImpl() override;
    void dispose();
    DECLARE_VIRTUAL_TRACE();

    void willBeDestroyed();
    WebDevToolsAgentClient* client() { return m_client; }
    InspectorOverlay* overlay() const { return m_overlay.get(); }
    void flushPendingProtocolNotifications();
    void dispatchMessageFromFrontend(int sessionId, const String& message);
    void registerAgent(PassOwnPtrWillBeRawPtr<InspectorAgent>);
    static void webViewImplClosed(WebViewImpl*);
    static void webFrameWidgetImplClosed(WebFrameWidgetImpl*);

    // Instrumentation from web/ layer.
    void didCommitLoadForLocalFrame(LocalFrame*);
    bool screencastEnabled();
    void willAddPageOverlay(const GraphicsLayer*);
    void didRemovePageOverlay(const GraphicsLayer*);
    void layerTreeViewChanged(WebLayerTreeView*);

    // WebDevToolsAgent implementation.
    void attach(const WebString& hostId, int sessionId) override;
    void reattach(const WebString& hostId, int sessionId, const WebString& savedState) override;
    void detach() override;
    void continueProgram() override;
    void dispatchOnInspectorBackend(int sessionId, const WebString& message) override;
    void inspectElementAt(const WebPoint&) override;
    void failedToRequestDevTools() override;
    void evaluateInWebInspector(long callId, const WebString& script) override;
    WebString evaluateInWebInspectorOverlay(const WebString& script) override;

private:
    WebDevToolsAgentImpl(WebLocalFrameImpl*, WebDevToolsAgentClient*, PassOwnPtrWillBeRawPtr<InspectorOverlay>);

    // InspectorTracingAgent::Client implementation.
    void enableTracing(const WTF::String& categoryFilter) override;
    void disableTracing() override;

    // InspectorEmulationAgent::Client implementation.
    void setCPUThrottlingRate(double) override;

    // InspectorRuntimeAgent::Client implementation.
    void resumeStartup() override;

    // InspectorPageAgent::Client implementation.
    void pageLayoutInvalidated() override;
    void setPausedInDebuggerMessage(const String*) override;
    void waitForCreateWindow(LocalFrame*) override;

    // InspectorFrontendChannel implementation.
    void sendProtocolResponse(int sessionId, int callId, PassRefPtr<JSONObject> message) override;
    void sendProtocolNotification(PassRefPtr<JSONObject> message) override;
    void flush() override;

    // WebThread::TaskObserver implementation.
    void willProcessTask() override;
    void didProcessTask() override;

    void initializeDeferredAgents();

    WebDevToolsAgentClient* m_client;
    RawPtrWillBeMember<WebLocalFrameImpl> m_webLocalFrameImpl;
    bool m_attached;
#if ENABLE(ASSERT)
    bool m_hasBeenDisposed;
#endif

    RefPtrWillBeMember<InstrumentingAgents> m_instrumentingAgents;
    OwnPtrWillBeMember<InspectorResourceContentLoader> m_resourceContentLoader;
    OwnPtrWillBeMember<InspectorOverlay> m_overlay;
    OwnPtrWillBeMember<InspectedFrames> m_inspectedFrames;

    RawPtrWillBeMember<InspectorInspectorAgent> m_inspectorAgent;
    RawPtrWillBeMember<InspectorDOMAgent> m_domAgent;
    RawPtrWillBeMember<InspectorPageAgent> m_pageAgent;
    RawPtrWillBeMember<InspectorResourceAgent> m_resourceAgent;
    RawPtrWillBeMember<InspectorLayerTreeAgent> m_layerTreeAgent;
    RawPtrWillBeMember<InspectorTracingAgent> m_tracingAgent;
    RawPtrWillBeMember<PageRuntimeAgent> m_pageRuntimeAgent;
    RawPtrWillBeMember<PageConsoleAgent> m_pageConsoleAgent;

    RefPtr<InspectorBackendDispatcher> m_inspectorBackendDispatcher;
    OwnPtr<InspectorFrontend> m_inspectorFrontend;
    InspectorAgentRegistry m_agents;
    bool m_deferredAgentsInitialized;

    typedef Vector<std::pair<int, RefPtr<JSONObject>>> NotificationQueue;
    NotificationQueue m_notificationQueue;
    int m_sessionId;
    String m_stateCookie;
    bool m_stateMuted;

    friend class DebuggerTask;
};

} // namespace blink

#endif

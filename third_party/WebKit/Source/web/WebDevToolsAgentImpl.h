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

#include "core/inspector/InspectorClient.h"
#include "core/inspector/InspectorFrontendChannel.h"

#include "public/platform/WebSize.h"
#include "public/platform/WebThread.h"
#include "public/web/WebDevToolsAgent.h"
#include "public/web/WebPageOverlay.h"
#include "wtf/Forward.h"
#include "wtf/OwnPtr.h"
#include "wtf/Vector.h"

namespace blink {

class LocalFrame;
class InspectorClient;
class InspectorController;
class IntPoint;
class Page;
class PlatformKeyboardEvent;
class WebDevToolsAgentClient;
class WebInputEvent;
class WebLocalFrameImpl;
class WebString;
class WebViewImpl;
class DebuggerTask;

class WebDevToolsAgentImpl final
    : public WebDevToolsAgent
    , public InspectorClient
    , public InspectorFrontendChannel
    , public WebPageOverlay
    , private WebThread::TaskObserver {
public:
    WebDevToolsAgentImpl(WebViewImpl* webViewImpl, WebDevToolsAgentClient* client);
    virtual ~WebDevToolsAgentImpl();

    WebDevToolsAgentClient* client() { return m_client; }

    bool handleInputEvent(Page*, const WebInputEvent&);

    // WebDevToolsAgent implementation.
    virtual void attach(const WebString& hostId) override;
    virtual void reattach(const WebString& hostId, const WebString& savedState) override;
    virtual void detach() override;
    virtual void continueProgram() override;
    virtual void dispatchOnInspectorBackend(const WebString& message) override;
    virtual void inspectElementAt(const WebPoint&) override;
    virtual void evaluateInWebInspector(long callId, const WebString& script) override;
    virtual void setLayerTreeId(int) override;
    virtual void processGPUEvent(const GPUEvent&) override;

    // InspectorClient implementation.
    virtual void highlight() override;
    virtual void hideHighlight() override;
    virtual void updateInspectorStateCookie(const WTF::String&) override;
    virtual void sendProtocolResponse(int callId, PassRefPtr<JSONObject> message) override;
    virtual void sendProtocolNotification(PassRefPtr<JSONObject> message) override;
    virtual void flush() override;
    virtual void resumeStartup() override;

    virtual void setDeviceMetricsOverride(int width, int height, float deviceScaleFactor, bool mobile, bool fitWindow, float scale, float offsetX, float offsetY) override;
    virtual void clearDeviceMetricsOverride() override;
    virtual void setTouchEventEmulationEnabled(bool) override;

    virtual void enableTracing(const WTF::String& categoryFilter) override;
    virtual void disableTracing() override;

    virtual void dispatchKeyEvent(const PlatformKeyboardEvent&) override;
    virtual void dispatchMouseEvent(const PlatformMouseEvent&) override;

    // WebPageOverlay
    virtual void paintPageOverlay(WebGraphicsContext*, const WebSize& webViewSize) override;

    void flushPendingProtocolNotifications();

private:
    // WebThread::TaskObserver
    virtual void willProcessTask() override;
    virtual void didProcessTask() override;

    void enableMobileEmulation();
    void disableMobileEmulation();

    InspectorController* inspectorController();
    LocalFrame* mainFrame();

    int m_layerTreeId;
    WebDevToolsAgentClient* m_client;
    WebViewImpl* m_webViewImpl;
    bool m_attached;
    bool m_generatingEvent;

    bool m_deviceMetricsEnabled;
    bool m_emulateMobileEnabled;
    bool m_originalViewportEnabled;
    bool m_isOverlayScrollbarsEnabled;

    float m_originalDefaultMinimumPageScaleFactor;
    float m_originalDefaultMaximumPageScaleFactor;

    bool m_touchEventEmulationEnabled;
    OwnPtr<IntPoint> m_lastPinchAnchorCss;
    OwnPtr<IntPoint> m_lastPinchAnchorDip;

    typedef Vector<RefPtr<JSONObject> > NotificationQueue;
    NotificationQueue m_notificationQueue;
    String m_stateCookie;

    friend class DebuggerTask;
};

} // namespace blink

#endif

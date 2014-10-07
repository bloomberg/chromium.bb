/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
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

#ifndef InspectorClientImpl_h
#define InspectorClientImpl_h

#include "core/inspector/InspectorClient.h"
#include "core/inspector/InspectorController.h"
#include "core/inspector/InspectorFrontendChannel.h"
#include "wtf/OwnPtr.h"

namespace blink {

class WebDevToolsAgentClient;
class WebDevToolsAgentImpl;
class WebViewImpl;

class InspectorClientImpl final : public InspectorClient, public InspectorFrontendChannel {
public:
    explicit InspectorClientImpl(WebViewImpl*);
    virtual ~InspectorClientImpl();

    // InspectorClient methods:
    virtual void highlight() override;
    virtual void hideHighlight() override;

    virtual void sendMessageToFrontend(PassRefPtr<JSONObject>) override;
    virtual void flush() override;

    virtual void updateInspectorStateCookie(const WTF::String&) override;

    virtual void setDeviceMetricsOverride(int, int, float, bool, bool, float, float, float) override;
    virtual void clearDeviceMetricsOverride() override;
    virtual void setTouchEventEmulationEnabled(bool) override;

    virtual bool overridesShowPaintRects() override;
    virtual void setShowPaintRects(bool) override;
    virtual void setShowDebugBorders(bool) override;
    virtual void setShowFPSCounter(bool) override;
    virtual void setContinuousPaintingEnabled(bool) override;
    virtual void setShowScrollBottleneckRects(bool) override;
    virtual void resetScrollAndPageScaleFactor() override;
    virtual float minimumPageScaleFactor() override;
    virtual float maximumPageScaleFactor() override;
    virtual void setPageScaleFactor(float) override;
    virtual void showContextMenu(float x, float y, PassRefPtr<ContextMenuProvider>) override;

    virtual void dispatchKeyEvent(const PlatformKeyboardEvent&) override;
    virtual void dispatchMouseEvent(const PlatformMouseEvent&) override;

    virtual void setTraceEventCallback(const String& categoryFilter, TraceEventCallback) override;
    virtual void resetTraceEventCallback() override;
    virtual void enableTracing(const String& categoryFilter) override;
    virtual void disableTracing() override;

    virtual void startGPUEventsRecording() override;
    virtual void stopGPUEventsRecording() override;

    virtual void resumeStartup() override;

private:
    WebDevToolsAgentImpl* devToolsAgent();

    // The WebViewImpl of the page being inspected; gets passed to the constructor
    WebViewImpl* m_inspectedWebView;
};

} // namespace blink

#endif

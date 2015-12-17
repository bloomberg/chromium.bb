/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef InspectorOverlay_h
#define InspectorOverlay_h

#include "core/InspectorTypeBuilder.h"
#include "core/inspector/InspectorDOMAgent.h"
#include "core/inspector/InspectorOverlayHost.h"
#include "core/inspector/InspectorPageAgent.h"
#include "core/inspector/InspectorProfilerAgent.h"
#include "platform/geometry/FloatQuad.h"
#include "platform/geometry/LayoutRect.h"
#include "platform/graphics/Color.h"
#include "platform/heap/Handle.h"
#include "public/web/WebInputEvent.h"
#include "wtf/OwnPtr.h"
#include "wtf/PassOwnPtr.h"
#include "wtf/RefPtr.h"
#include "wtf/text/WTFString.h"

namespace blink {

class Color;
class EmptyChromeClient;
class LocalFrame;
class GraphicsContext;
class GraphicsLayer;
class InspectorCSSAgent;
class InspectorDebuggerAgent;
class JSONValue;
class LayoutEditor;
class Node;
class Page;
class PageOverlay;
class WebViewImpl;

class InspectorOverlay final
    : public NoBaseWillBeGarbageCollectedFinalized<InspectorOverlay>
    , public InspectorDOMAgent::Client
    , public InspectorPageAgent::Client
    , public InspectorProfilerAgent::Client
    , public InspectorOverlayHost::Listener {
    USING_FAST_MALLOC_WILL_BE_REMOVED(InspectorOverlay);
    WILL_BE_USING_GARBAGE_COLLECTED_MIXIN(InspectorOverlay);
public:
    static PassOwnPtrWillBeRawPtr<InspectorOverlay> create(WebViewImpl* webViewImpl)
    {
        return adoptPtrWillBeNoop(new InspectorOverlay(webViewImpl));
    }

    ~InspectorOverlay() override;
    DECLARE_TRACE();

    void init(InspectorCSSAgent*, InspectorDebuggerAgent*, InspectorDOMAgent*);

    void clear();
    bool handleInputEvent(const WebInputEvent&);

    // Does not yet include paint.
    void updateAllLifecyclePhases();

    PageOverlay* pageOverlay() { return m_pageOverlay.get(); };
    String evaluateInOverlayForTest(const String&);

private:
    explicit InspectorOverlay(WebViewImpl*);
    class InspectorOverlayChromeClient;
    class InspectorPageOverlayDelegate;

    // InspectorOverlayHost::Listener implementation.
    void overlayResumed() override;
    void overlaySteppedOver() override;
    void overlayStartedPropertyChange(const String&) override;
    void overlayPropertyChanged(float) override;
    void overlayEndedPropertyChange() override;
    void overlayClearSelection(bool) override;
    void overlayNextSelector() override;
    void overlayPreviousSelector() override;

    // InspectorProfilerAgent::Client implementation.
    void profilingStarted() override;
    void profilingStopped() override;

    // InspectorPageAgent::Client implementation.
    void pageLayoutInvalidated() override;
    void setPausedInDebuggerMessage(const String*) override;

    // InspectorDOMAgent::Client implementation.
    void hideHighlight() override;
    void highlightNode(Node*, const InspectorHighlightConfig&, bool omitTooltip) override;
    void highlightQuad(PassOwnPtr<FloatQuad>, const InspectorHighlightConfig&) override;
    void setInspectMode(InspectorDOMAgent::SearchMode, PassOwnPtr<InspectorHighlightConfig>) override;
    void setInspectedNode(Node*) override;

    void highlightNode(Node*, Node* eventTarget, const InspectorHighlightConfig&, bool omitTooltip);
    bool isEmpty();
    void drawNodeHighlight();
    void drawQuadHighlight();
    void drawPausedInDebuggerMessage();

    Page* overlayPage();
    LocalFrame* overlayMainFrame();
    void reset(const IntSize& viewportSize, const IntPoint& documentScrollOffset);
    void evaluateInOverlay(const String& method, const String& argument);
    void evaluateInOverlay(const String& method, PassRefPtr<JSONValue> argument);
    void rebuildOverlayPage();
    void invalidate();
    void scheduleUpdate();

    bool handleMousePress();
    bool handleGestureEvent(const PlatformGestureEvent&);
    bool handleTouchEvent(const PlatformTouchEvent&);
    bool handleMouseMove(const PlatformMouseEvent&);
    bool shouldSearchForNode();
    void inspect(Node*);
    void initializeLayoutEditorIfNeeded(Node*);

    WebViewImpl* m_webViewImpl;
    String m_pausedInDebuggerMessage;
    RefPtrWillBeMember<Node> m_highlightNode;
    RefPtrWillBeMember<Node> m_eventTargetNode;
    InspectorHighlightConfig m_nodeHighlightConfig;
    OwnPtr<FloatQuad> m_highlightQuad;
    OwnPtrWillBeMember<Page> m_overlayPage;
    OwnPtrWillBeMember<InspectorOverlayChromeClient> m_overlayChromeClient;
    RefPtrWillBeMember<InspectorOverlayHost> m_overlayHost;
    InspectorHighlightConfig m_quadHighlightConfig;
    bool m_omitTooltip;
    int m_suspendCount;
    bool m_inLayout;
    bool m_needsUpdate;
    RawPtrWillBeMember<InspectorDebuggerAgent> m_debuggerAgent;
    RawPtrWillBeMember<InspectorDOMAgent> m_domAgent;
    RawPtrWillBeMember<InspectorCSSAgent> m_cssAgent;
    OwnPtrWillBeMember<LayoutEditor> m_layoutEditor;
    OwnPtr<PageOverlay> m_pageOverlay;
    RefPtrWillBeMember<Node> m_hoveredNodeForInspectMode;
    InspectorDOMAgent::SearchMode m_inspectMode;
    OwnPtr<InspectorHighlightConfig> m_inspectModeHighlightConfig;
};

} // namespace blink


#endif // InspectorOverlay_h

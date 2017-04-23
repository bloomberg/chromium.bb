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

#include <v8-inspector.h>
#include <memory>
#include "core/inspector/InspectorDOMAgent.h"
#include "core/inspector/InspectorOverlayHost.h"
#include "core/inspector/protocol/Forward.h"
#include "platform/Timer.h"
#include "platform/geometry/FloatQuad.h"
#include "platform/geometry/LayoutRect.h"
#include "platform/graphics/Color.h"
#include "platform/heap/Handle.h"
#include "platform/wtf/RefPtr.h"
#include "platform/wtf/text/WTFString.h"
#include "public/platform/WebInputEvent.h"

namespace blink {

class Color;
class LocalFrame;
class Node;
class Page;
class PageOverlay;
class WebGestureEvent;
class WebMouseEvent;
class WebLocalFrameImpl;
class WebTouchEvent;

namespace protocol {
class Value;
}

class InspectorOverlay final
    : public GarbageCollectedFinalized<InspectorOverlay>,
      public InspectorDOMAgent::Client,
      public InspectorOverlayHost::Listener {
  USING_GARBAGE_COLLECTED_MIXIN(InspectorOverlay);

 public:
  explicit InspectorOverlay(WebLocalFrameImpl*);
  ~InspectorOverlay() override;
  DECLARE_TRACE();

  void Init(v8_inspector::V8InspectorSession*, InspectorDOMAgent*);

  void Clear();
  void Suspend();
  void Resume();
  bool HandleInputEvent(const WebInputEvent&);
  void PageLayoutInvalidated(bool resized);
  void SetShowViewportSizeOnResize(bool);
  void ShowReloadingBlanket();
  void HideReloadingBlanket();
  void SetPausedInDebuggerMessage(const String&);

  // Does not yet include paint.
  void UpdateAllLifecyclePhases();

  PageOverlay* GetPageOverlay() { return page_overlay_.get(); };
  String EvaluateInOverlayForTest(const String&);

 private:
  class InspectorOverlayChromeClient;
  class InspectorPageOverlayDelegate;

  // InspectorOverlayHost::Listener implementation.
  void OverlayResumed() override;
  void OverlaySteppedOver() override;

  // InspectorDOMAgent::Client implementation.
  void HideHighlight() override;
  void HighlightNode(Node*,
                     const InspectorHighlightConfig&,
                     bool omit_tooltip) override;
  void HighlightQuad(std::unique_ptr<FloatQuad>,
                     const InspectorHighlightConfig&) override;
  void SetInspectMode(InspectorDOMAgent::SearchMode,
                      std::unique_ptr<InspectorHighlightConfig>) override;

  void HighlightNode(Node*,
                     Node* event_target,
                     const InspectorHighlightConfig&,
                     bool omit_tooltip);
  bool IsEmpty();
  void DrawNodeHighlight();
  void DrawQuadHighlight();
  void DrawPausedInDebuggerMessage();
  void DrawViewSize();

  float WindowToViewportScale() const;

  Page* OverlayPage();
  LocalFrame* OverlayMainFrame();
  void Reset(const IntSize& viewport_size,
             const IntPoint& document_scroll_offset);
  void EvaluateInOverlay(const String& method, const String& argument);
  void EvaluateInOverlay(const String& method,
                         std::unique_ptr<protocol::Value> argument);
  void OnTimer(TimerBase*);
  void RebuildOverlayPage();
  void Invalidate();
  void ScheduleUpdate();
  void ClearInternal();

  bool HandleMouseDown();
  bool HandleMouseUp();
  bool HandleGestureEvent(const WebGestureEvent&);
  bool HandleTouchEvent(const WebTouchEvent&);
  bool HandleMouseMove(const WebMouseEvent&);
  bool ShouldSearchForNode();
  void Inspect(Node*);

  Member<WebLocalFrameImpl> frame_impl_;
  String paused_in_debugger_message_;
  Member<Node> highlight_node_;
  Member<Node> event_target_node_;
  InspectorHighlightConfig node_highlight_config_;
  std::unique_ptr<FloatQuad> highlight_quad_;
  Member<Page> overlay_page_;
  Member<InspectorOverlayChromeClient> overlay_chrome_client_;
  Member<InspectorOverlayHost> overlay_host_;
  InspectorHighlightConfig quad_highlight_config_;
  bool draw_view_size_;
  bool resize_timer_active_;
  bool omit_tooltip_;
  TaskRunnerTimer<InspectorOverlay> timer_;
  bool suspended_;
  bool show_reloading_blanket_;
  bool in_layout_;
  bool needs_update_;
  v8_inspector::V8InspectorSession* v8_session_;
  Member<InspectorDOMAgent> dom_agent_;
  std::unique_ptr<PageOverlay> page_overlay_;
  Member<Node> hovered_node_for_inspect_mode_;
  bool swallow_next_mouse_up_;
  InspectorDOMAgent::SearchMode inspect_mode_;
  std::unique_ptr<InspectorHighlightConfig> inspect_mode_highlight_config_;
};

}  // namespace blink

#endif  // InspectorOverlay_h

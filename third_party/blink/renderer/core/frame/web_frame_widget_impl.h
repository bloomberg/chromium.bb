/*
 * Copyright (C) 2014 Google Inc. All rights reserved.
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

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_WEB_FRAME_WIDGET_IMPL_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_WEB_FRAME_WIDGET_IMPL_H_

#include <memory>

#include "base/memory/scoped_refptr.h"
#include "base/optional.h"
#include "base/single_thread_task_runner.h"
#include "base/util/type_safety/pass_key.h"
#include "third_party/blink/public/common/input/web_coalesced_input_event.h"
#include "third_party/blink/public/mojom/scroll/scroll_into_view_params.mojom-blink-forward.h"
#include "third_party/blink/public/platform/web_size.h"
#include "third_party/blink/public/web/web_input_method_controller.h"
#include "third_party/blink/renderer/core/frame/web_frame_widget_base.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/core/page/page_widget_delegate.h"
#include "third_party/blink/renderer/core/scroll/scroll_types.h"
#include "third_party/blink/renderer/platform/graphics/apply_viewport_changes.h"
#include "third_party/blink/renderer/platform/graphics/graphics_layer.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/heap/self_keep_alive.h"
#include "third_party/blink/renderer/platform/wtf/hash_set.h"

namespace cc {
class Layer;
struct BeginMainFrameMetrics;
}

namespace blink {
class Frame;
class Element;
class HTMLPlugInElement;
class LocalFrame;
class PaintLayerCompositor;
class WebFrameWidget;
class WebMouseEvent;
class WebMouseWheelEvent;
class WebFrameWidgetImpl;

class WebFrameWidgetImpl final : public WebFrameWidgetBase,
                                 public PageWidgetEventHandler {
 public:
  WebFrameWidgetImpl(
      util::PassKey<WebFrameWidget>,
      WebWidgetClient&,
      CrossVariantMojoAssociatedRemote<
          mojom::blink::FrameWidgetHostInterfaceBase> frame_widget_host,
      CrossVariantMojoAssociatedReceiver<mojom::blink::FrameWidgetInterfaceBase>
          frame_widget,
      CrossVariantMojoAssociatedRemote<mojom::blink::WidgetHostInterfaceBase>
          widget_host,
      CrossVariantMojoAssociatedReceiver<mojom::blink::WidgetInterfaceBase>
          widget);
  ~WebFrameWidgetImpl() override;

  // WebWidget functions:
  void Close(scoped_refptr<base::SingleThreadTaskRunner> cleanup_runner,
             base::OnceCallback<void()> cleanup_task) override;
  WebSize Size() override;
  void Resize(const WebSize&) override;
  void DidEnterFullscreen() override;
  void DidExitFullscreen() override;
  void UpdateLifecycle(WebLifecycleUpdate requested_update,
                       DocumentUpdateReason reason) override;
  void ThemeChanged() override;
  WebHitTestResult HitTestResultAt(const gfx::PointF&) override;
  WebInputEventResult DispatchBufferedTouchEvents() override;
  WebInputEventResult HandleInputEvent(const WebCoalescedInputEvent&) override;
  void SetCursorVisibilityState(bool is_visible) override;

  void MouseCaptureLost() override;
  void SetFocus(bool enable) override;
  bool SelectionBounds(WebRect& anchor, WebRect& focus) const override;
  void SetRemoteViewportIntersection(const ViewportIntersectionState&) override;
  void SetIsInertForSubFrame(bool) override;
  void SetInheritedEffectiveTouchActionForSubFrame(TouchAction) override;
  void UpdateRenderThrottlingStatusForSubFrame(bool is_throttled,
                                               bool subtree_throttled) override;
  WebURL GetURLForDebugTrace() override;

  // WebFrameWidget implementation.
  void DidDetachLocalFrameTree() override;
  WebInputMethodController* GetActiveWebInputMethodController() const override;
  bool ScrollFocusedEditableElementIntoView() override;

  Frame* FocusedCoreFrame() const;

  // Returns the currently focused Element or null if no element has focus.
  Element* FocusedElement() const;

  PaintLayerCompositor* Compositor() const;

  // WebFrameWidgetBase overrides:
  bool ForSubframe() const override { return true; }
  void IntrinsicSizingInfoChanged(
      mojom::blink::IntrinsicSizingInfoPtr) override;
  void DidCreateLocalRootView() override;
  HitTestResult CoreHitTestResultAt(const gfx::PointF&) override;
  void ZoomToFindInPageRect(const WebRect& rect_in_root_frame) override;

  // FrameWidget overrides:
  void SetRootLayer(scoped_refptr<cc::Layer>) override;

  // WidgetBaseClient overrides:
  void BeginMainFrame(base::TimeTicks last_frame_time) override;
  void SetSuppressFrameRequestsWorkaroundFor704763Only(bool) final;
  void RecordStartOfFrameMetrics() override;
  void RecordEndOfFrameMetrics(
      base::TimeTicks,
      cc::ActiveFrameSequenceTrackers trackers) override;
  std::unique_ptr<cc::BeginMainFrameMetrics> GetBeginMainFrameMetrics()
      override;
  void BeginUpdateLayers() override;
  void EndUpdateLayers() override;
  void BeginCommitCompositorFrame() override;
  void EndCommitCompositorFrame(base::TimeTicks commit_start_time) override;
  void DidBeginMainFrame() override;

  void UpdateMainFrameLayoutSize();

  // Event related methods:
  void MouseContextMenu(const WebMouseEvent&);

  void Trace(Visitor*) override;

 private:
  friend class WebFrameWidget;  // For WebFrameWidget::create.

  // Perform a hit test for a point relative to the root frame of the page.
  HitTestResult HitTestResultForRootFramePos(
      const FloatPoint& pos_in_root_frame);

  void UpdateLayerTreeViewport();

  // PageWidgetEventHandler functions
  void HandleMouseLeave(LocalFrame&, const WebMouseEvent&) override;
  void HandleMouseDown(LocalFrame&, const WebMouseEvent&) override;
  WebInputEventResult HandleMouseUp(LocalFrame&, const WebMouseEvent&) override;
  WebInputEventResult HandleMouseWheel(LocalFrame&,
                                       const WebMouseWheelEvent&) override;
  WebInputEventResult HandleGestureEvent(const WebGestureEvent&) override;
  WebInputEventResult HandleKeyEvent(const WebKeyboardEvent&) override;
  WebInputEventResult HandleCharEvent(const WebKeyboardEvent&) override;

  PageWidgetEventHandler* GetPageWidgetEventHandler() override;
  LocalFrameView* GetLocalFrameViewForAnimationScrolling() override;

  LocalFrame* FocusedLocalFrameAvailableForIme() const;

  // Finds the parameters required for scrolling the focused editable |element|
  // into view. |rect_to_scroll| is used for recursive scrolling of the element
  // into view and contains all or part of element's bounding box and always
  // includes the caret and is with respect to absolute coordinates.
  void GetScrollParamsForFocusedEditableElement(
      const Element& element,
      PhysicalRect& rect_to_scroll,
      mojom::blink::ScrollIntoViewParamsPtr& params);

  base::Optional<WebSize> size_;

  // If set, the (plugin) element which has mouse capture.
  Member<HTMLPlugInElement> mouse_capture_element_;

  // Metrics gathering timing information
  base::Optional<base::TimeTicks> update_layers_start_time_;
  base::Optional<base::TimeTicks> commit_compositor_frame_start_time_;

  bool suppress_next_keypress_event_ = false;

  bool did_suspend_parsing_ = false;

  // TODO(ekaramad): Can we remove this and make sure IME events are not called
  // when there is no page focus?
  // Represents whether or not this object should process incoming IME events.
  bool ime_accept_events_ = true;

  SelfKeepAlive<WebFrameWidgetImpl> self_keep_alive_;
};

}  // namespace blink

#endif

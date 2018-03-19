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

#ifndef WebFrameWidgetImpl_h
#define WebFrameWidgetImpl_h

#include <memory>

#include "base/memory/scoped_refptr.h"
#include "core/frame/WebFrameWidgetBase.h"
#include "core/frame/WebLocalFrameImpl.h"
#include "core/page/PageWidgetDelegate.h"
#include "platform/graphics/CompositorMutatorImpl.h"
#include "platform/graphics/GraphicsLayer.h"
#include "platform/heap/Member.h"
#include "platform/heap/SelfKeepAlive.h"
#include "platform/scroll/ScrollTypes.h"
#include "platform/wtf/HashSet.h"
#include "platform/wtf/Optional.h"
#include "public/platform/WebCoalescedInputEvent.h"
#include "public/platform/WebPoint.h"
#include "public/platform/WebSize.h"
#include "public/web/WebInputMethodController.h"

namespace blink {

class CompositorAnimationHost;
class Frame;
class Element;
class LocalFrame;
class PaintLayerCompositor;
class UserGestureToken;
class WebLayer;
class WebLayerTreeView;
class WebMouseEvent;
class WebMouseWheelEvent;
class WebFrameWidgetImpl;

using WebFrameWidgetsSet =
    PersistentHeapHashSet<WeakMember<WebFrameWidgetImpl>>;

class WebFrameWidgetImpl final : public WebFrameWidgetBase,
                                 public PageWidgetEventHandler {
 public:
  static WebFrameWidgetImpl* Create(WebWidgetClient*, WebLocalFrame*);

  ~WebFrameWidgetImpl();

  // WebWidget functions:
  void Close() override;
  WebSize Size() override;
  void Resize(const WebSize&) override;
  void ResizeVisualViewport(const WebSize&) override;
  void DidEnterFullscreen() override;
  void DidExitFullscreen() override;
  void SetSuppressFrameRequestsWorkaroundFor704763Only(bool) final;
  void BeginFrame(double last_frame_time_monotonic) override;
  void UpdateLifecycle(LifecycleUpdate requested_update) override;
  void UpdateAllLifecyclePhasesAndCompositeForTesting() override;
  void Paint(WebCanvas*, const WebRect&) override;
  void LayoutAndPaintAsync(WebLayoutAndPaintAsyncCallback*) override;
  void CompositeAndReadbackAsync(
      WebCompositeAndReadbackAsyncCallback*) override;
  void ThemeChanged() override;
  WebHitTestResult HitTestResultAt(const WebPoint&) override;
  WebInputEventResult DispatchBufferedTouchEvents() override;
  WebInputEventResult HandleInputEvent(const WebCoalescedInputEvent&) override;
  void SetCursorVisibilityState(bool is_visible) override;

  void ApplyViewportDeltas(const WebFloatSize& visual_viewport_delta,
                           const WebFloatSize& main_frame_delta,
                           const WebFloatSize& elastic_overscroll_delta,
                           float page_scale_delta,
                           float browser_controls_delta) override;
  void MouseCaptureLost() override;
  void SetFocus(bool enable) override;
  WebColor BackgroundColor() const override;
  bool SelectionBounds(WebRect& anchor, WebRect& focus) const override;
  bool IsAcceleratedCompositingActive() const override;
  void WillCloseLayerTreeView() override;
  void SetRemoteViewportIntersection(const WebRect&) override;
  void SetIsInert(bool) override;
  void UpdateRenderThrottlingStatus(bool is_throttled,
                                    bool subtree_throttled) override;

  // WebFrameWidget implementation.
  WebLocalFrameImpl* LocalRoot() const override { return local_root_; }
  void SetVisibilityState(mojom::PageVisibilityState) override;
  void SetBackgroundColorOverride(WebColor) override;
  void ClearBackgroundColorOverride() override;
  void SetBaseBackgroundColorOverride(WebColor) override;
  void ClearBaseBackgroundColorOverride() override;
  void SetBaseBackgroundColor(WebColor) override;
  WebInputMethodController* GetActiveWebInputMethodController() const override;

  Frame* FocusedCoreFrame() const;

  // Returns the currently focused Element or null if no element has focus.
  Element* FocusedElement() const;

  PaintLayerCompositor* Compositor() const;
  // Create or return cached mutation distributor.  This usually requires a
  // round trip to the compositor.  The output task runner is the one to use
  // for sending mutations using the WeakPtr.
  base::WeakPtr<CompositorMutatorImpl> EnsureCompositorMutator(
      scoped_refptr<base::SingleThreadTaskRunner>* mutator_task_runner)
      override;

  // WebFrameWidgetBase overrides:
  bool ForSubframe() const override { return true; }
  void ScheduleAnimation() override;
  void IntrinsicSizingInfoChanged(const IntrinsicSizingInfo&) override;
  void DidCreateLocalRootView() override;

  WebWidgetClient* Client() const override { return client_; }
  void SetRootGraphicsLayer(GraphicsLayer*) override;
  void SetRootLayer(WebLayer*) override;
  WebLayerTreeView* GetLayerTreeView() const override;
  CompositorAnimationHost* AnimationHost() const override;
  HitTestResult CoreHitTestResultAt(const WebPoint&) override;

  // Exposed for the purpose of overriding device metrics.
  void SendResizeEventAndRepaint();

  void UpdateMainFrameLayoutSize();

  // Event related methods:
  void MouseContextMenu(const WebMouseEvent&);

  GraphicsLayer* RootGraphicsLayer() const override {
    return root_graphics_layer_;
  };

  Color BaseBackgroundColor() const;

  void Trace(blink::Visitor*);

 private:
  friend class WebFrameWidget;  // For WebFrameWidget::create.

  explicit WebFrameWidgetImpl(WebWidgetClient*, WebLocalFrame*);

  // Perform a hit test for a point relative to the root frame of the page.
  HitTestResult HitTestResultForRootFramePos(
      const LayoutPoint& pos_in_root_frame);

  void InitializeLayerTreeView();

  void SetIsAcceleratedCompositingActive(bool);
  void UpdateLayerTreeViewport();
  void UpdateLayerTreeBackgroundColor();
  void UpdateBaseBackgroundColor();

  // PageWidgetEventHandler functions
  void HandleMouseLeave(LocalFrame&, const WebMouseEvent&) override;
  void HandleMouseDown(LocalFrame&, const WebMouseEvent&) override;
  void HandleMouseUp(LocalFrame&, const WebMouseEvent&) override;
  WebInputEventResult HandleMouseWheel(LocalFrame&,
                                       const WebMouseWheelEvent&) override;
  WebInputEventResult HandleGestureEvent(const WebGestureEvent&) override;
  WebInputEventResult HandleKeyEvent(const WebKeyboardEvent&) override;
  WebInputEventResult HandleCharEvent(const WebKeyboardEvent&) override;

  PageWidgetEventHandler* GetPageWidgetEventHandler() override;

  LocalFrame* FocusedLocalFrameAvailableForIme() const;

  WebWidgetClient* client_;

  // WebFrameWidget is associated with a subtree of the frame tree,
  // corresponding to a maximal connected tree of LocalFrames. This member
  // points to the root of that subtree.
  Member<WebLocalFrameImpl> local_root_;

  WTF::Optional<WebSize> size_;

  // If set, the (plugin) node which has mouse capture.
  Member<Node> mouse_capture_node_;
  scoped_refptr<UserGestureToken> mouse_capture_gesture_token_;

  // This is owned by the LayerTreeHostImpl, and should only be used on the
  // compositor thread, so we keep the TaskRunner where you post tasks to
  // make that happen.
  base::WeakPtr<CompositorMutatorImpl> mutator_;
  scoped_refptr<base::SingleThreadTaskRunner> mutator_task_runner_;

  WebLayerTreeView* layer_tree_view_;
  WebLayer* root_layer_;
  GraphicsLayer* root_graphics_layer_;
  std::unique_ptr<CompositorAnimationHost> animation_host_;
  bool is_accelerated_compositing_active_;
  bool layer_tree_view_closed_;

  bool suppress_next_keypress_event_;

  bool did_suspend_parsing_ = false;

  bool background_color_override_enabled_;
  WebColor background_color_override_;
  bool base_background_color_override_enabled_;
  WebColor base_background_color_override_;

  // TODO(ekaramad): Can we remove this and make sure IME events are not called
  // when there is no page focus?
  // Represents whether or not this object should process incoming IME events.
  bool ime_accept_events_;

  WebColor base_background_color_;

  SelfKeepAlive<WebFrameWidgetImpl> self_keep_alive_;
};

DEFINE_TYPE_CASTS(WebFrameWidgetImpl,
                  WebFrameWidgetBase,
                  widget,
                  widget->ForSubframe(),
                  widget.ForSubframe());

}  // namespace blink

#endif

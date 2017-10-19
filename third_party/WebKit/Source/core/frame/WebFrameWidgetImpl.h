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

#include "core/animation/CompositorMutatorImpl.h"
#include "core/frame/WebFrameWidgetBase.h"
#include "core/frame/WebLocalFrameImpl.h"
#include "core/page/PageWidgetDelegate.h"
#include "platform/graphics/GraphicsLayer.h"
#include "platform/heap/SelfKeepAlive.h"
#include "platform/scroll/ScrollTypes.h"
#include "platform/wtf/Assertions.h"
#include "platform/wtf/HashSet.h"
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
  void UpdateAllLifecyclePhases() override;
  void Paint(WebCanvas*, const WebRect&) override;
  void LayoutAndPaintAsync(WebLayoutAndPaintAsyncCallback*) override;
  void CompositeAndReadbackAsync(
      WebCompositeAndReadbackAsyncCallback*) override;
  void ThemeChanged() override;
  WebInputEventResult HandleInputEvent(const WebCoalescedInputEvent&) override;
  void SetCursorVisibilityState(bool is_visible) override;
  bool HasTouchEventHandlersAt(const WebPoint&) override;

  void ApplyViewportDeltas(const WebFloatSize& visual_viewport_delta,
                           const WebFloatSize& main_frame_delta,
                           const WebFloatSize& elastic_overscroll_delta,
                           float page_scale_delta,
                           float browser_controls_delta) override;
  void MouseCaptureLost() override;
  void SetFocus(bool enable) override;
  WebRange CompositionRange() override;
  WebColor BackgroundColor() const override;
  bool SelectionBounds(WebRect& anchor, WebRect& focus) const override;
  bool SelectionTextDirection(WebTextDirection& start,
                              WebTextDirection& end) const override;
  bool IsSelectionAnchorFirst() const override;
  void SetTextDirection(WebTextDirection) override;
  bool IsAcceleratedCompositingActive() const override;
  void WillCloseLayerTreeView() override;
  bool GetCompositionCharacterBounds(WebVector<WebRect>& bounds) override;
  void SetRemoteViewportIntersection(const WebRect&) override;
  void SetIsInert(bool) override;

  // WebFrameWidget implementation.
  WebLocalFrameImpl* LocalRoot() const override { return local_root_; }
  void SetVisibilityState(WebPageVisibilityState) override;
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
  CompositorMutatorImpl* CompositorMutator() override;

  // WebFrameWidgetBase overrides:
  bool ForSubframe() const override { return true; }
  void ScheduleAnimation() override;

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
  HitTestResult HitTestResultForRootFramePos(const IntPoint& pos_in_root_frame);

  void InitializeLayerTreeView();

  void SetIsAcceleratedCompositingActive(bool);
  void UpdateLayerTreeViewport();
  void UpdateLayerTreeBackgroundColor();
  void UpdateLayerTreeDeviceScaleFactor();
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

  // This method returns the focused frame belonging to this WebWidget, that
  // is, a focused frame with the same local root as the one corresponding
  // to this widget. It will return nullptr if no frame is focused or, the
  // focused frame has a different local root.
  LocalFrame* FocusedLocalFrameInWidget() const;

  LocalFrame* FocusedLocalFrameAvailableForIme() const;

  CompositorMutatorImpl& Mutator();

  WebWidgetClient* client_;

  // WebFrameWidget is associated with a subtree of the frame tree,
  // corresponding to a maximal connected tree of LocalFrames. This member
  // points to the root of that subtree.
  Member<WebLocalFrameImpl> local_root_;

  WebSize size_;

  // If set, the (plugin) node which has mouse capture.
  Member<Node> mouse_capture_node_;
  RefPtr<UserGestureToken> mouse_capture_gesture_token_;

  // This is owned by the LayerTreeHostImpl, and should only be used on the
  // compositor thread. The LayerTreeHostImpl is indirectly owned by this
  // class so this pointer should be valid until this class is destructed.
  CrossThreadPersistent<CompositorMutatorImpl> mutator_;

  WebLayerTreeView* layer_tree_view_;
  WebLayer* root_layer_;
  GraphicsLayer* root_graphics_layer_;
  std::unique_ptr<CompositorAnimationHost> animation_host_;
  bool is_accelerated_compositing_active_;
  bool layer_tree_view_closed_;

  bool suppress_next_keypress_event_;

  bool background_color_override_enabled_;
  WebColor background_color_override_;
  bool base_background_color_override_enabled_;
  WebColor base_background_color_override_;

  // TODO(ekaramad): Can we remove this and make sure IME events are not called
  // when there is no page focus?
  // Represents whether or not this object should process incoming IME events.
  bool ime_accept_events_;

  static const WebInputEvent* current_input_event_;

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

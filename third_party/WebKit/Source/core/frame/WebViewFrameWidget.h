// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

#ifndef WebViewFrameWidget_h
#define WebViewFrameWidget_h

#include "core/CoreExport.h"
#include "core/frame/WebFrameWidgetBase.h"
#include "core/frame/WebLocalFrameImpl.h"
#include "platform/heap/Handle.h"
#include "platform/heap/SelfKeepAlive.h"
#include "platform/wtf/Noncopyable.h"
#include "platform/wtf/RefPtr.h"

namespace blink {

class WebViewImpl;
class WebWidgetClient;

// Shim class to help normalize the widget interfaces in the Blink public API.
// For OOPI, subframes have WebFrameWidgets for input and rendering.
// Unfortunately, the main frame still uses WebView's WebWidget for input and
// rendering. This results in complex code, since there are two different
// implementations of WebWidget and code needs to have branches to handle both
// cases.
// This class allows a Blink embedder to create a WebFrameWidget that can be
// used for the main frame. Internally, it currently wraps WebView's WebWidget
// and just forwards almost everything to it.
// After the embedder starts using a WebFrameWidget for the main frame,
// WebView will be updated to no longer inherit WebWidget. The eventual goal is
// to unfork the widget code duplicated in WebFrameWidgetImpl and WebViewImpl
// into one class.
// A more detailed writeup of this transition can be read at
// https://goo.gl/7yVrnb.
class CORE_EXPORT WebViewFrameWidget : public WebFrameWidgetBase {
  WTF_MAKE_NONCOPYABLE(WebViewFrameWidget);

 public:
  explicit WebViewFrameWidget(WebWidgetClient&,
                              WebViewImpl&,
                              WebLocalFrameImpl&);
  virtual ~WebViewFrameWidget();

  // WebFrameWidget overrides:
  void Close() override;
  WebSize Size() override;
  void Resize(const WebSize&) override;
  void ResizeVisualViewport(const WebSize&) override;
  void DidEnterFullscreen() override;
  void DidExitFullscreen() override;
  void SetSuppressFrameRequestsWorkaroundFor704763Only(bool) final;
  void BeginFrame(double last_frame_time_monotonic) override;
  void UpdateAllLifecyclePhases() override;
  void Paint(WebCanvas*, const WebRect& view_port) override;
  void LayoutAndPaintAsync(WebLayoutAndPaintAsyncCallback*) override;
  void CompositeAndReadbackAsync(
      WebCompositeAndReadbackAsyncCallback*) override;
  void ThemeChanged() override;
  WebInputEventResult HandleInputEvent(const WebCoalescedInputEvent&) override;
  void SetCursorVisibilityState(bool is_visible) override;
  bool HasTouchEventHandlersAt(const WebPoint&) override;
  void ApplyViewportDeltas(const WebFloatSize& visual_viewport_delta,
                           const WebFloatSize& layout_viewport_delta,
                           const WebFloatSize& elastic_overscroll_delta,
                           float scale_factor,
                           float browser_controls_shown_ratio_delta) override;
  void RecordWheelAndTouchScrollingCount(bool has_scrolled_by_wheel,
                                         bool has_scrolled_by_touch) override;
  void MouseCaptureLost() override;
  void SetFocus(bool) override;
  WebRange CompositionRange() override;
  bool SelectionBounds(WebRect& anchor, WebRect& focus) const override;
  bool SelectionTextDirection(WebTextDirection& start,
                              WebTextDirection& end) const override;
  bool IsSelectionAnchorFirst() const override;
  void SetTextDirection(WebTextDirection) override;
  bool IsAcceleratedCompositingActive() const override;
  bool IsWebView() const override { return false; }
  bool IsPagePopup() const override { return false; }
  void WillCloseLayerTreeView() override;
  WebColor BackgroundColor() const override;
  WebPagePopup* GetPagePopup() const override;
  bool GetCompositionCharacterBounds(WebVector<WebRect>& bounds) override;
  void UpdateBrowserControlsState(WebBrowserControlsState constraints,
                                  WebBrowserControlsState current,
                                  bool animate) override;
  void SetVisibilityState(WebPageVisibilityState) override;
  void SetBackgroundColorOverride(WebColor) override;
  void ClearBackgroundColorOverride() override;
  void SetBaseBackgroundColorOverride(WebColor) override;
  void ClearBaseBackgroundColorOverride() override;
  void SetBaseBackgroundColor(WebColor) override;
  WebLocalFrameImpl* LocalRoot() const override;
  WebInputMethodController* GetActiveWebInputMethodController() const override;

  // WebFrameWidgetBase overrides:
  bool ForSubframe() const override { return false; }
  void ScheduleAnimation() override;
  CompositorMutatorImpl* CompositorMutator() override;
  void SetRootGraphicsLayer(GraphicsLayer*) override;
  GraphicsLayer* RootGraphicsLayer() const override;
  void SetRootLayer(WebLayer*) override;
  WebLayerTreeView* GetLayerTreeView() const override;
  CompositorAnimationHost* AnimationHost() const override;
  WebWidgetClient* Client() const override { return client_; }
  HitTestResult CoreHitTestResultAt(const WebPoint&) override;

  virtual void Trace(blink::Visitor*);

 private:
  PageWidgetEventHandler* GetPageWidgetEventHandler() override;

  WebWidgetClient* client_;
  scoped_refptr<WebViewImpl> web_view_;
  Member<WebLocalFrameImpl> main_frame_;

  SelfKeepAlive<WebViewFrameWidget> self_keep_alive_;
};

}  // namespace blink

#endif  // WebViewFrameWidget_h

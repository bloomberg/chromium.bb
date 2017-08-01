// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

#include "core/frame/WebViewFrameWidget.h"

#include "core/exported/WebViewBase.h"
#include "core/frame/WebLocalFrameImpl.h"
#include "core/layout/HitTestResult.h"

namespace blink {

WebViewFrameWidget::WebViewFrameWidget(WebWidgetClient& client,
                                       WebViewBase& web_view,
                                       WebLocalFrameImpl& main_frame)
    : client_(&client),
      web_view_(&web_view),
      main_frame_(&main_frame),
      self_keep_alive_(this) {
  main_frame_->SetFrameWidget(this);
  web_view_->SetCompositorVisibility(true);
}

WebViewFrameWidget::~WebViewFrameWidget() {}

void WebViewFrameWidget::Close() {
  // Note: it's important to use the captured main frame pointer here. During
  // a frame swap, the swapped frame is detached *after* the frame tree is
  // updated. If the main frame is being swapped, then
  // m_webView()->mainFrameImpl() will no longer point to the original frame.
  web_view_->SetCompositorVisibility(false);
  main_frame_->SetFrameWidget(nullptr);
  main_frame_ = nullptr;
  web_view_ = nullptr;
  client_ = nullptr;

  // Note: this intentionally does not forward to WebView::close(), to make it
  // easier to untangle the cleanup logic later.
  self_keep_alive_.Clear();
}

WebSize WebViewFrameWidget::Size() {
  return web_view_->Size();
}

void WebViewFrameWidget::Resize(const WebSize& size) {
  return web_view_->Resize(size);
}

void WebViewFrameWidget::ResizeVisualViewport(const WebSize& size) {
  return web_view_->ResizeVisualViewport(size);
}

void WebViewFrameWidget::DidEnterFullscreen() {
  return web_view_->DidEnterFullscreen();
}

void WebViewFrameWidget::DidExitFullscreen() {
  return web_view_->DidExitFullscreen();
}

void WebViewFrameWidget::SetSuppressFrameRequestsWorkaroundFor704763Only(
    bool suppress_frame_requests) {
  return web_view_->SetSuppressFrameRequestsWorkaroundFor704763Only(
      suppress_frame_requests);
}
void WebViewFrameWidget::BeginFrame(double last_frame_time_monotonic) {
  return web_view_->BeginFrame(last_frame_time_monotonic);
}

void WebViewFrameWidget::UpdateAllLifecyclePhases() {
  return web_view_->UpdateAllLifecyclePhases();
}

void WebViewFrameWidget::Paint(WebCanvas* canvas, const WebRect& view_port) {
  return web_view_->Paint(canvas, view_port);
}

void WebViewFrameWidget::LayoutAndPaintAsync(
    WebLayoutAndPaintAsyncCallback* callback) {
  return web_view_->LayoutAndPaintAsync(callback);
}

void WebViewFrameWidget::CompositeAndReadbackAsync(
    WebCompositeAndReadbackAsyncCallback* callback) {
  return web_view_->CompositeAndReadbackAsync(callback);
}

void WebViewFrameWidget::ThemeChanged() {
  return web_view_->ThemeChanged();
}

WebInputEventResult WebViewFrameWidget::HandleInputEvent(
    const WebCoalescedInputEvent& event) {
  return web_view_->HandleInputEvent(event);
}

void WebViewFrameWidget::SetCursorVisibilityState(bool is_visible) {
  return web_view_->SetCursorVisibilityState(is_visible);
}

bool WebViewFrameWidget::HasTouchEventHandlersAt(const WebPoint& point) {
  return web_view_->HasTouchEventHandlersAt(point);
}

void WebViewFrameWidget::ApplyViewportDeltas(
    const WebFloatSize& visual_viewport_delta,
    const WebFloatSize& layout_viewport_delta,
    const WebFloatSize& elastic_overscroll_delta,
    float scale_factor,
    float browser_controls_shown_ratio_delta) {
  return web_view_->ApplyViewportDeltas(
      visual_viewport_delta, layout_viewport_delta, elastic_overscroll_delta,
      scale_factor, browser_controls_shown_ratio_delta);
}

void WebViewFrameWidget::RecordWheelAndTouchScrollingCount(
    bool has_scrolled_by_wheel,
    bool has_scrolled_by_touch) {
  return web_view_->RecordWheelAndTouchScrollingCount(has_scrolled_by_wheel,
                                                      has_scrolled_by_touch);
}

void WebViewFrameWidget::MouseCaptureLost() {
  return web_view_->MouseCaptureLost();
}

void WebViewFrameWidget::SetFocus(bool enable) {
  return web_view_->SetFocus(enable);
}

WebRange WebViewFrameWidget::CompositionRange() {
  return web_view_->CompositionRange();
}

bool WebViewFrameWidget::SelectionBounds(WebRect& anchor,
                                         WebRect& focus) const {
  return web_view_->SelectionBounds(anchor, focus);
}

bool WebViewFrameWidget::SelectionTextDirection(WebTextDirection& start,
                                                WebTextDirection& end) const {
  return web_view_->SelectionTextDirection(start, end);
}

bool WebViewFrameWidget::IsSelectionAnchorFirst() const {
  return web_view_->IsSelectionAnchorFirst();
}

void WebViewFrameWidget::SetTextDirection(WebTextDirection direction) {
  return web_view_->SetTextDirection(direction);
}

bool WebViewFrameWidget::IsAcceleratedCompositingActive() const {
  return web_view_->IsAcceleratedCompositingActive();
}

void WebViewFrameWidget::WillCloseLayerTreeView() {
  return web_view_->WillCloseLayerTreeView();
}

WebColor WebViewFrameWidget::BackgroundColor() const {
  return web_view_->BackgroundColor();
}

WebPagePopup* WebViewFrameWidget::GetPagePopup() const {
  return web_view_->GetPagePopup();
}

bool WebViewFrameWidget::GetCompositionCharacterBounds(
    WebVector<WebRect>& bounds) {
  return web_view_->GetCompositionCharacterBounds(bounds);
}

void WebViewFrameWidget::UpdateBrowserControlsState(
    WebBrowserControlsState constraints,
    WebBrowserControlsState current,
    bool animate) {
  return web_view_->UpdateBrowserControlsState(constraints, current, animate);
}

void WebViewFrameWidget::SetVisibilityState(
    WebPageVisibilityState visibility_state) {
  return web_view_->SetVisibilityState(visibility_state, false);
}

void WebViewFrameWidget::SetBackgroundColorOverride(WebColor color) {
  web_view_->SetBackgroundColorOverride(color);
}

void WebViewFrameWidget::ClearBackgroundColorOverride() {
  return web_view_->ClearBackgroundColorOverride();
}

void WebViewFrameWidget::SetBaseBackgroundColorOverride(WebColor color) {
  web_view_->SetBaseBackgroundColorOverride(color);
}

void WebViewFrameWidget::ClearBaseBackgroundColorOverride() {
  return web_view_->ClearBaseBackgroundColorOverride();
}

void WebViewFrameWidget::SetBaseBackgroundColor(WebColor color) {
  web_view_->SetBaseBackgroundColor(color);
}

WebLocalFrameImpl* WebViewFrameWidget::LocalRoot() const {
  return web_view_->MainFrameImpl();
}

WebInputMethodController*
WebViewFrameWidget::GetActiveWebInputMethodController() const {
  return web_view_->GetActiveWebInputMethodController();
}

void WebViewFrameWidget::ScheduleAnimation() {
  web_view_->ScheduleAnimationForWidget();
}

CompositorMutatorImpl* WebViewFrameWidget::CompositorMutator() {
  return web_view_->CompositorMutator();
}

void WebViewFrameWidget::SetRootGraphicsLayer(GraphicsLayer* layer) {
  web_view_->SetRootGraphicsLayer(layer);
}

GraphicsLayer* WebViewFrameWidget::RootGraphicsLayer() const {
  return web_view_->RootGraphicsLayer();
}

void WebViewFrameWidget::SetRootLayer(WebLayer* layer) {
  web_view_->SetRootLayer(layer);
}

WebLayerTreeView* WebViewFrameWidget::GetLayerTreeView() const {
  return web_view_->LayerTreeView();
}

CompositorAnimationHost* WebViewFrameWidget::AnimationHost() const {
  return web_view_->AnimationHost();
}

HitTestResult WebViewFrameWidget::CoreHitTestResultAt(const WebPoint& point) {
  return web_view_->CoreHitTestResultAt(point);
}

DEFINE_TRACE(WebViewFrameWidget) {
  visitor->Trace(main_frame_);
  WebFrameWidgetBase::Trace(visitor);
}

}  // namespace blink

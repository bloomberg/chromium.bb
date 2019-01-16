// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

#include "third_party/blink/renderer/core/frame/web_view_frame_widget.h"

#include "third_party/blink/renderer/core/exported/web_view_impl.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"

namespace blink {

WebViewFrameWidget::WebViewFrameWidget(WebWidgetClient& client,
                                       WebViewImpl& web_view)
    : WebFrameWidgetBase(client), web_view_(&web_view), self_keep_alive_(this) {
  // TODO(danakj): SetLayerTreeView() here as well, then we can Close() the
  // WebViewImpl's widget bits in Close().
  web_view_->SetWebWidgetClient(&client);
}

WebViewFrameWidget::~WebViewFrameWidget() = default;

void WebViewFrameWidget::Close() {
  // TODO(danakj): Close() the WebViewImpl here, when we reset the LayerTreeView
  // in the constructor.
  web_view_->SetWebWidgetClient(nullptr);
  web_view_ = nullptr;
  WebFrameWidgetBase::Close();

  // Note: this intentionally does not forward to WebView::close(), to make it
  // easier to untangle the cleanup logic later.
  self_keep_alive_.Clear();
}

WebSize WebViewFrameWidget::Size() {
  return web_view_->Size();
}

void WebViewFrameWidget::Resize(const WebSize& size) {
  web_view_->Resize(size);
}

void WebViewFrameWidget::ResizeVisualViewport(const WebSize& size) {
  web_view_->ResizeVisualViewport(size);
}

void WebViewFrameWidget::DidEnterFullscreen() {
  web_view_->DidEnterFullscreen();
}

void WebViewFrameWidget::DidExitFullscreen() {
  web_view_->DidExitFullscreen();
}

void WebViewFrameWidget::SetSuppressFrameRequestsWorkaroundFor704763Only(
    bool suppress_frame_requests) {
  web_view_->SetSuppressFrameRequestsWorkaroundFor704763Only(
      suppress_frame_requests);
}
void WebViewFrameWidget::BeginFrame(base::TimeTicks last_frame_time) {
  web_view_->BeginFrame(last_frame_time);
}

void WebViewFrameWidget::RecordEndOfFrameMetrics(
    base::TimeTicks frame_begin_time) {
  web_view_->RecordEndOfFrameMetrics(frame_begin_time);
}

void WebViewFrameWidget::UpdateLifecycle(LifecycleUpdate requested_update,
                                         LifecycleUpdateReason reason) {
  web_view_->UpdateLifecycle(requested_update, reason);
}

void WebViewFrameWidget::PaintContent(cc::PaintCanvas* canvas,
                                      const WebRect& view_port) {
  web_view_->PaintContent(canvas, view_port);
}

void WebViewFrameWidget::CompositeAndReadbackAsync(
    base::OnceCallback<void(const SkBitmap&)> callback) {
  web_view_->CompositeAndReadbackAsync(std::move(callback));
}

void WebViewFrameWidget::ThemeChanged() {
  web_view_->ThemeChanged();
}

WebInputEventResult WebViewFrameWidget::HandleInputEvent(
    const WebCoalescedInputEvent& event) {
  return web_view_->HandleInputEvent(event);
}

WebInputEventResult WebViewFrameWidget::DispatchBufferedTouchEvents() {
  return web_view_->DispatchBufferedTouchEvents();
}

void WebViewFrameWidget::SetCursorVisibilityState(bool is_visible) {
  web_view_->SetCursorVisibilityState(is_visible);
}

void WebViewFrameWidget::ApplyViewportChanges(
    const ApplyViewportChangesArgs& args) {
  web_view_->ApplyViewportChanges(args);
}

void WebViewFrameWidget::RecordWheelAndTouchScrollingCount(
    bool has_scrolled_by_wheel,
    bool has_scrolled_by_touch) {
  web_view_->RecordWheelAndTouchScrollingCount(has_scrolled_by_wheel,
                                               has_scrolled_by_touch);
}
void WebViewFrameWidget::SendOverscrollEventFromImplSide(
    const gfx::Vector2dF& overscroll_delta,
    cc::ElementId scroll_latched_element_id) {
  web_view_->SendOverscrollEventFromImplSide(overscroll_delta,
                                             scroll_latched_element_id);
}
void WebViewFrameWidget::SendScrollEndEventFromImplSide(
    cc::ElementId scroll_latched_element_id) {
  web_view_->SendScrollEndEventFromImplSide(scroll_latched_element_id);
}

void WebViewFrameWidget::MouseCaptureLost() {
  web_view_->MouseCaptureLost();
}

void WebViewFrameWidget::SetFocus(bool enable) {
  web_view_->SetFocus(enable);
}

bool WebViewFrameWidget::SelectionBounds(WebRect& anchor,
                                         WebRect& focus) const {
  return web_view_->SelectionBounds(anchor, focus);
}

bool WebViewFrameWidget::IsAcceleratedCompositingActive() const {
  return web_view_->IsAcceleratedCompositingActive();
}

void WebViewFrameWidget::WillCloseLayerTreeView() {
  web_view_->WillCloseLayerTreeView();
}

WebURL WebViewFrameWidget::GetURLForDebugTrace() {
  return web_view_->GetURLForDebugTrace();
}

WebInputMethodController*
WebViewFrameWidget::GetActiveWebInputMethodController() const {
  return web_view_->GetActiveWebInputMethodController();
}

bool WebViewFrameWidget::ScrollFocusedEditableElementIntoView() {
  return web_view_->ScrollFocusedEditableElementIntoView();
}

void WebViewFrameWidget::Initialize() {}

void WebViewFrameWidget::SetLayerTreeView(WebLayerTreeView*) {
  // The WebViewImpl already has its LayerTreeView, the WebWidgetClient
  // thus does not initialize and set another one here.
  NOTREACHED();
}

base::WeakPtr<AnimationWorkletMutatorDispatcherImpl>
WebViewFrameWidget::EnsureCompositorMutatorDispatcher(
    scoped_refptr<base::SingleThreadTaskRunner>* mutator_task_runner) {
  return web_view_->EnsureCompositorMutatorDispatcher(mutator_task_runner);
}

void WebViewFrameWidget::SetRootGraphicsLayer(GraphicsLayer* layer) {
  web_view_->SetRootGraphicsLayer(layer);
}

GraphicsLayer* WebViewFrameWidget::RootGraphicsLayer() const {
  return web_view_->RootGraphicsLayer();
}

void WebViewFrameWidget::SetRootLayer(scoped_refptr<cc::Layer> layer) {
  web_view_->SetRootLayer(layer);
}

WebLayerTreeView* WebViewFrameWidget::GetLayerTreeView() const {
  return web_view_->LayerTreeView();
}

CompositorAnimationHost* WebViewFrameWidget::AnimationHost() const {
  return web_view_->AnimationHost();
}

WebHitTestResult WebViewFrameWidget::HitTestResultAt(const gfx::Point& point) {
  return web_view_->HitTestResultAt(point);
}

HitTestResult WebViewFrameWidget::CoreHitTestResultAt(const gfx::Point& point) {
  return web_view_->CoreHitTestResultAt(point);
}

void WebViewFrameWidget::ZoomToFindInPageRect(
    const WebRect& rect_in_root_frame) {
  web_view_->ZoomToFindInPageRect(rect_in_root_frame);
}

void WebViewFrameWidget::Trace(blink::Visitor* visitor) {
  WebFrameWidgetBase::Trace(visitor);
}

PageWidgetEventHandler* WebViewFrameWidget::GetPageWidgetEventHandler() {
  return web_view_.get();
}

}  // namespace blink

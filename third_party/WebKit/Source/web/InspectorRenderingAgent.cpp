// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "web/InspectorRenderingAgent.h"

#include "core/frame/FrameView.h"
#include "core/frame/Settings.h"
#include "core/page/Page.h"
#include "web/InspectorOverlay.h"
#include "web/WebLocalFrameImpl.h"
#include "web/WebViewImpl.h"

namespace blink {

using protocol::Response;

namespace RenderingAgentState {
static const char kShowDebugBorders[] = "showDebugBorders";
static const char kShowFPSCounter[] = "showFPSCounter";
static const char kShowPaintRects[] = "showPaintRects";
static const char kShowScrollBottleneckRects[] = "showScrollBottleneckRects";
static const char kShowSizeOnResize[] = "showSizeOnResize";
}

InspectorRenderingAgent* InspectorRenderingAgent::Create(
    WebLocalFrameImpl* web_local_frame_impl,
    InspectorOverlay* overlay) {
  return new InspectorRenderingAgent(web_local_frame_impl, overlay);
}

InspectorRenderingAgent::InspectorRenderingAgent(
    WebLocalFrameImpl* web_local_frame_impl,
    InspectorOverlay* overlay)
    : web_local_frame_impl_(web_local_frame_impl), overlay_(overlay) {}

WebViewImpl* InspectorRenderingAgent::GetWebViewImpl() {
  return web_local_frame_impl_->ViewImpl();
}

void InspectorRenderingAgent::Restore() {
  setShowDebugBorders(
      state_->booleanProperty(RenderingAgentState::kShowDebugBorders, false));
  setShowFPSCounter(
      state_->booleanProperty(RenderingAgentState::kShowFPSCounter, false));
  setShowPaintRects(
      state_->booleanProperty(RenderingAgentState::kShowPaintRects, false));
  setShowScrollBottleneckRects(state_->booleanProperty(
      RenderingAgentState::kShowScrollBottleneckRects, false));
  setShowViewportSizeOnResize(
      state_->booleanProperty(RenderingAgentState::kShowSizeOnResize, false));
}

Response InspectorRenderingAgent::disable() {
  setShowDebugBorders(false);
  setShowFPSCounter(false);
  setShowPaintRects(false);
  setShowScrollBottleneckRects(false);
  setShowViewportSizeOnResize(false);
  return Response::OK();
}

Response InspectorRenderingAgent::setShowDebugBorders(bool show) {
  state_->setBoolean(RenderingAgentState::kShowDebugBorders, show);
  if (show) {
    Response response = CompositingEnabled();
    if (!response.isSuccess())
      return response;
  }
  GetWebViewImpl()->SetShowDebugBorders(show);
  return Response::OK();
}

Response InspectorRenderingAgent::setShowFPSCounter(bool show) {
  state_->setBoolean(RenderingAgentState::kShowFPSCounter, show);
  if (show) {
    Response response = CompositingEnabled();
    if (!response.isSuccess())
      return response;
  }
  GetWebViewImpl()->SetShowFPSCounter(show);
  return Response::OK();
}

Response InspectorRenderingAgent::setShowPaintRects(bool show) {
  state_->setBoolean(RenderingAgentState::kShowPaintRects, show);
  GetWebViewImpl()->SetShowPaintRects(show);
  if (!show && web_local_frame_impl_->GetFrameView())
    web_local_frame_impl_->GetFrameView()->Invalidate();
  return Response::OK();
}

Response InspectorRenderingAgent::setShowScrollBottleneckRects(bool show) {
  state_->setBoolean(RenderingAgentState::kShowScrollBottleneckRects, show);
  if (show) {
    Response response = CompositingEnabled();
    if (!response.isSuccess())
      return response;
  }
  GetWebViewImpl()->SetShowScrollBottleneckRects(show);
  return Response::OK();
}

Response InspectorRenderingAgent::setShowViewportSizeOnResize(bool show) {
  state_->setBoolean(RenderingAgentState::kShowSizeOnResize, show);
  if (overlay_)
    overlay_->SetShowViewportSizeOnResize(show);
  return Response::OK();
}

Response InspectorRenderingAgent::CompositingEnabled() {
  if (!GetWebViewImpl()
           ->GetPage()
           ->GetSettings()
           .GetAcceleratedCompositingEnabled())
    return Response::Error("Compositing mode is not supported");
  return Response::OK();
}

DEFINE_TRACE(InspectorRenderingAgent) {
  visitor->Trace(web_local_frame_impl_);
  visitor->Trace(overlay_);
  InspectorBaseAgent::Trace(visitor);
}

}  // namespace blink

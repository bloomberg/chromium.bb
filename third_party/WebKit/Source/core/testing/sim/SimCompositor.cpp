// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/testing/sim/SimCompositor.h"

#include "core/exported/WebViewImpl.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/LocalFrameView.h"
#include "core/frame/WebLocalFrameImpl.h"
#include "core/layout/LayoutView.h"
#include "core/paint/PaintLayer.h"
#include "core/paint/compositing/CompositedLayerMapping.h"
#include "core/paint/compositing/PaintLayerCompositor.h"
#include "platform/graphics/GraphicsContext.h"
#include "platform/graphics/paint/CullRect.h"
#include "platform/graphics/paint/PaintRecordBuilder.h"
#include "platform/wtf/Time.h"
#include "public/platform/WebRect.h"

namespace blink {

SimCompositor::SimCompositor()
    : needs_begin_frame_(false),
      defer_commits_(true),
      has_selection_(false),
      web_view_(nullptr),
      last_frame_time_monotonic_(0) {
  LocalFrameView::SetInitialTracksPaintInvalidationsForTesting(true);
}

SimCompositor::~SimCompositor() {
  LocalFrameView::SetInitialTracksPaintInvalidationsForTesting(false);
}

void SimCompositor::SetWebView(WebViewImpl& web_view) {
  web_view_ = &web_view;
}

void SimCompositor::SetNeedsBeginFrame() {
  needs_begin_frame_ = true;
}

void SimCompositor::SetDeferCommits(bool defer_commits) {
  defer_commits_ = defer_commits;
}

void SimCompositor::RegisterSelection(const WebSelection&) {
  has_selection_ = true;
}

void SimCompositor::ClearSelection() {
  has_selection_ = false;
}

SimCanvas::Commands SimCompositor::BeginFrame(double time_delta_in_seconds) {
  DCHECK(web_view_);
  DCHECK(!defer_commits_);
  DCHECK(needs_begin_frame_);
  DCHECK_GT(time_delta_in_seconds, 0);
  needs_begin_frame_ = false;

  last_frame_time_monotonic_ += time_delta_in_seconds;

  web_view_->BeginFrame(last_frame_time_monotonic_);
  web_view_->UpdateAllLifecyclePhases();

  return PaintFrame();
}

SimCanvas::Commands SimCompositor::PaintFrame() {
  DCHECK(web_view_);

  auto* frame = web_view_->MainFrameImpl()->GetFrame();
  DocumentLifecycle::AllowThrottlingScope throttling_scope(
      frame->GetDocument()->Lifecycle());
  PaintRecordBuilder builder;
  auto infinite_rect = LayoutRect::InfiniteIntRect();
  frame->View()->Paint(builder.Context(), kGlobalPaintFlattenCompositingLayers,
                       CullRect(infinite_rect));

  SimCanvas canvas(infinite_rect.Width(), infinite_rect.Height());
  builder.EndRecording()->Playback(&canvas);
  return canvas.GetCommands();
}

}  // namespace blink

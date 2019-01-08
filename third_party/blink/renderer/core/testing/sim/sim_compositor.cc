// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/testing/sim/sim_compositor.h"

#include "cc/test/fake_layer_tree_frame_sink.h"
#include "third_party/blink/public/platform/web_rect.h"
#include "third_party/blink/renderer/core/exported/web_view_impl.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/paint/compositing/composited_layer_mapping.h"
#include "third_party/blink/renderer/core/paint/compositing/paint_layer_compositor.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context.h"
#include "third_party/blink/renderer/platform/graphics/paint/cull_rect.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_record_builder.h"
#include "third_party/blink/renderer/platform/wtf/time.h"

namespace blink {

SimCompositor::SimCompositor() {
  LocalFrameView::SetInitialTracksPaintInvalidationsForTesting(true);
}

SimCompositor::~SimCompositor() {
  LocalFrameView::SetInitialTracksPaintInvalidationsForTesting(false);
}

void SimCompositor::SetWebView(
    WebViewImpl& web_view,
    content::LayerTreeView& layer_tree_view,
    frame_test_helpers::TestWebViewClient& view_client,
    frame_test_helpers::TestWebWidgetClient& widget_client) {
  web_view_ = &web_view;
  layer_tree_view_ = &layer_tree_view;
  DCHECK_EQ(&layer_tree_view, web_view_->LayerTreeView());
  test_web_view_client_ = &view_client;
  DCHECK_EQ(test_web_view_client_, web_view_->Client());
  test_web_widget_client_ = &widget_client;
  DCHECK_EQ(test_web_widget_client_, web_view_->WidgetClient());

  // SimCompositor starts with defer commits enabled, but uses synchronous
  // compositing which does not use defer commits anyhow, it only uses it for
  // reading deferred state in tests.
  web_view_->DeferMainFrameUpdateForTesting();
}

SimCanvas::Commands SimCompositor::BeginFrame(double time_delta_in_seconds) {
  DCHECK(web_view_);
  DCHECK(!layer_tree_view_->layer_tree_host()->defer_main_frame_update());
  // Verify that the need for a BeginMainFrame has been registered, and would
  // have caused the compositor to schedule one if we were using its scheduler.
  DCHECK(NeedsBeginFrame());
  DCHECK_GT(time_delta_in_seconds, 0);

  test_web_widget_client_->ClearAnimationScheduled();

  last_frame_time_ += base::TimeDelta::FromSecondsD(time_delta_in_seconds);

  SimCanvas::Commands commands;
  paint_commands_ = &commands;

  layer_tree_view_->layer_tree_host()->Composite(last_frame_time_,
                                                 /*raster=*/false);

  paint_commands_ = nullptr;
  return commands;
}

SimCanvas::Commands SimCompositor::PaintFrame() {
  DCHECK(web_view_);

  auto* frame = web_view_->MainFrameImpl()->GetFrame();
  DocumentLifecycle::AllowThrottlingScope throttling_scope(
      frame->GetDocument()->Lifecycle());
  frame->View()->UpdateAllLifecyclePhases(DocumentLifecycle::kTest);
  PaintRecordBuilder builder;
  frame->View()->PaintOutsideOfLifecycle(builder.Context(),
                                         kGlobalPaintFlattenCompositingLayers);

  auto infinite_rect = LayoutRect::InfiniteIntRect();
  SimCanvas canvas(infinite_rect.Width(), infinite_rect.Height());
  builder.EndRecording()->Playback(&canvas);
  return canvas.GetCommands();
}

void SimCompositor::ApplyViewportChanges(const ApplyViewportChangesArgs& args) {
  web_view_->MainFrameWidget()->ApplyViewportChanges(args);
}

void SimCompositor::RequestNewLayerTreeFrameSink(
    LayerTreeFrameSinkCallback callback) {
  // Make a valid LayerTreeFrameSink so the compositor will generate begin main
  // frames.
  std::move(callback).Run(cc::FakeLayerTreeFrameSink::Create3d());
}

void SimCompositor::BeginMainFrame(base::TimeTicks frame_time) {
  // There is no WebWidget like RenderWidget would have..? So go right to the
  // WebViewImpl.
  web_view_->MainFrameWidget()->BeginFrame(last_frame_time_);
  web_view_->MainFrameWidget()->UpdateAllLifecyclePhases(
      WebWidget::LifecycleUpdateReason::kTest);
  *paint_commands_ = PaintFrame();
}

}  // namespace blink

// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/gfx/display_webview.h"

#include "android_webview/browser/gfx/overlay_processor_webview.h"
#include "base/memory/ptr_util.h"
#include "components/viz/service/display/overlay_processor_stub.h"
#include "components/viz/service/frame_sinks/frame_sink_manager_impl.h"
#include "gpu/config/gpu_finch_features.h"

namespace android_webview {

std::unique_ptr<DisplayWebView> DisplayWebView::Create(
    const viz::RendererSettings& settings,
    const viz::DebugRendererSettings* debug_settings,
    const viz::FrameSinkId& frame_sink_id,
    std::unique_ptr<viz::DisplayCompositorMemoryAndTaskController>
        gpu_dependency,
    std::unique_ptr<viz::OutputSurface> output_surface,
    viz::FrameSinkManagerImpl* frame_sink_manager,
    RootFrameSink* root_frame_sink) {
  std::unique_ptr<viz::OverlayProcessorInterface> overlay_processor;
  OverlayProcessorWebView* overlay_processor_webview_raw = nullptr;
  if (features::IsAndroidSurfaceControlEnabled()) {
    auto overlay_processor_webview = std::make_unique<OverlayProcessorWebView>(
        gpu_dependency.get(), frame_sink_manager);
    overlay_processor_webview_raw = overlay_processor_webview.get();
    overlay_processor = std::move(overlay_processor_webview);
  } else {
    overlay_processor = std::make_unique<viz::OverlayProcessorStub>();
  }

  auto scheduler = std::make_unique<DisplaySchedulerWebView>(
      root_frame_sink, overlay_processor_webview_raw);

  return base::WrapUnique(new DisplayWebView(
      settings, debug_settings, frame_sink_id, std::move(gpu_dependency),
      std::move(output_surface), std::move(overlay_processor),
      std::move(scheduler), overlay_processor_webview_raw, frame_sink_manager));
}

DisplayWebView::DisplayWebView(
    const viz::RendererSettings& settings,
    const viz::DebugRendererSettings* debug_settings,
    const viz::FrameSinkId& frame_sink_id,
    std::unique_ptr<viz::DisplayCompositorMemoryAndTaskController>
        gpu_dependency,
    std::unique_ptr<viz::OutputSurface> output_surface,
    std::unique_ptr<viz::OverlayProcessorInterface> overlay_processor,
    std::unique_ptr<viz::DisplaySchedulerBase> scheduler,
    OverlayProcessorWebView* overlay_processor_webview,
    viz::FrameSinkManagerImpl* frame_sink_manager)
    : viz::Display(/*bitmap_manager=*/nullptr,
                   settings,
                   debug_settings,
                   frame_sink_id,
                   std::move(gpu_dependency),
                   std::move(output_surface),
                   std::move(overlay_processor),
                   std::move(scheduler),
                   /*current_task_runner=*/nullptr),
      overlay_processor_webview_(overlay_processor_webview) {
  if (overlay_processor_webview_) {
    frame_sink_manager_observation_.Observe(frame_sink_manager);
  }
}

DisplayWebView::~DisplayWebView() = default;

void DisplayWebView::OnFrameSinkDidFinishFrame(
    const viz::FrameSinkId& frame_sink_id,
    const viz::BeginFrameArgs& args) {
  DCHECK(overlay_processor_webview_);
  auto surface_id =
      overlay_processor_webview_->GetOverlaySurfaceId(frame_sink_id);
  if (surface_id.is_valid()) {
    // TODO(vasilyt): We don't need full aggregation here as we don't need
    // aggregated frame.
    aggregator_->Aggregate(current_surface_id_, base::TimeTicks::Now(),
                           gfx::OVERLAY_TRANSFORM_NONE, gfx::Rect(),
                           ++swapped_trace_id_);
    auto* resolved_data = aggregator_->GetLatestFrameData(surface_id);
    if (resolved_data) {
      overlay_processor_webview_->ProcessForFrameSinkId(frame_sink_id,
                                                        resolved_data);
    }
  }
}

}  // namespace android_webview

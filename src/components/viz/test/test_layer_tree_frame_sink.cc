// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/test/test_layer_tree_frame_sink.h"

#include <stdint.h>

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/single_thread_task_runner.h"
#include "cc/trees/layer_tree_frame_sink_client.h"
#include "components/viz/common/frame_sinks/begin_frame_args.h"
#include "components/viz/service/display/direct_renderer.h"
#include "components/viz/service/display/output_surface.h"
#include "components/viz/service/display/skia_output_surface.h"
#include "components/viz/service/frame_sinks/compositor_frame_sink_support.h"
#include "mojo/public/cpp/system/platform_handle.h"

namespace viz {

static constexpr FrameSinkId kLayerTreeFrameSinkId(1, 1);

TestLayerTreeFrameSink::TestLayerTreeFrameSink(
    scoped_refptr<ContextProvider> compositor_context_provider,
    scoped_refptr<RasterContextProvider> worker_context_provider,
    gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager,
    const RendererSettings& renderer_settings,
    scoped_refptr<base::SingleThreadTaskRunner> compositor_task_runner,
    bool synchronous_composite,
    bool disable_display_vsync,
    double refresh_rate,
    BeginFrameSource* begin_frame_source)
    : LayerTreeFrameSink(std::move(compositor_context_provider),
                         std::move(worker_context_provider),
                         std::move(compositor_task_runner),
                         gpu_memory_buffer_manager),
      synchronous_composite_(synchronous_composite),
      disable_display_vsync_(disable_display_vsync),
      renderer_settings_(renderer_settings),
      refresh_rate_(refresh_rate),
      frame_sink_id_(kLayerTreeFrameSinkId),
      parent_local_surface_id_allocator_(new ParentLocalSurfaceIdAllocator),
      client_provided_begin_frame_source_(begin_frame_source),
      external_begin_frame_source_(this),
      weak_ptr_factory_(this) {
  parent_local_surface_id_allocator_->GenerateId();
}

TestLayerTreeFrameSink::~TestLayerTreeFrameSink() = default;

void TestLayerTreeFrameSink::SetDisplayColorSpace(
    const gfx::ColorSpace& blending_color_space,
    const gfx::ColorSpace& output_color_space) {
  blending_color_space_ = blending_color_space;
  output_color_space_ = output_color_space;
  if (display_)
    display_->SetColorSpace(blending_color_space_, output_color_space_);
}

bool TestLayerTreeFrameSink::BindToClient(
    cc::LayerTreeFrameSinkClient* client) {
  if (!LayerTreeFrameSink::BindToClient(client))
    return false;

  shared_bitmap_manager_ = std::make_unique<TestSharedBitmapManager>();
  frame_sink_manager_ =
      std::make_unique<FrameSinkManagerImpl>(shared_bitmap_manager_.get());

  std::unique_ptr<OutputSurface> display_output_surface;
  SkiaOutputSurface* display_skia_output_surface = nullptr;
  if (renderer_settings_.use_skia_renderer) {
    auto output_surface = test_client_->CreateDisplaySkiaOutputSurface();
    display_skia_output_surface = output_surface.get();
    display_output_surface = std::move(output_surface);
  } else {
    display_output_surface =
        test_client_->CreateDisplayOutputSurface(context_provider());
  }

  std::unique_ptr<DisplayScheduler> scheduler;
  if (!synchronous_composite_) {
    if (client_provided_begin_frame_source_) {
      display_begin_frame_source_ = client_provided_begin_frame_source_;
    } else if (disable_display_vsync_) {
      begin_frame_source_ = std::make_unique<BackToBackBeginFrameSource>(
          std::make_unique<DelayBasedTimeSource>(
              compositor_task_runner_.get()));
      display_begin_frame_source_ = begin_frame_source_.get();
    } else {
      begin_frame_source_ = std::make_unique<DelayBasedBeginFrameSource>(
          std::make_unique<DelayBasedTimeSource>(compositor_task_runner_.get()),
          BeginFrameSource::kNotRestartableId);
      begin_frame_source_->OnUpdateVSyncParameters(
          base::TimeTicks::Now(),
          base::TimeDelta::FromMilliseconds(1000.f / refresh_rate_));
      display_begin_frame_source_ = begin_frame_source_.get();
    }
    scheduler = std::make_unique<DisplayScheduler>(
        display_begin_frame_source_, compositor_task_runner_.get(),
        display_output_surface->capabilities().max_frames_pending);
  }

  display_ = std::make_unique<Display>(
      shared_bitmap_manager_.get(), renderer_settings_, frame_sink_id_,
      std::move(display_output_surface), std::move(scheduler),
      compositor_task_runner_, display_skia_output_surface);

  constexpr bool is_root = true;
  constexpr bool needs_sync_points = true;
  support_ = std::make_unique<CompositorFrameSinkSupport>(
      this, frame_sink_manager_.get(), frame_sink_id_, is_root,
      needs_sync_points);
  support_->SetWantsAnimateOnlyBeginFrames();
  client_->SetBeginFrameSource(&external_begin_frame_source_);
  if (display_begin_frame_source_) {
    frame_sink_manager_->RegisterBeginFrameSource(display_begin_frame_source_,
                                                  frame_sink_id_);
  }
  display_->Initialize(this, frame_sink_manager_->surface_manager());
  display_->renderer_for_testing()->SetEnlargePassTextureAmountForTesting(
      enlarge_pass_texture_amount_);
  display_->SetColorSpace(blending_color_space_, output_color_space_);
  display_->SetVisible(true);
  return true;
}

void TestLayerTreeFrameSink::DetachFromClient() {
  // This acts like the |shared_bitmap_manager_| is a global object, while
  // in fact it is tied to the lifetime of this class and is destroyed below:
  // The shared_bitmap_manager_ has ownership of shared memory for each
  // SharedBitmapId that has been reported from the client. Since the client is
  // gone that memory can be freed. If we don't then it would leak.
  for (const auto& id : owned_bitmaps_)
    shared_bitmap_manager_->ChildDeletedSharedBitmap(id);
  owned_bitmaps_.clear();

  if (display_begin_frame_source_) {
    frame_sink_manager_->UnregisterBeginFrameSource(
        display_begin_frame_source_);
    display_begin_frame_source_ = nullptr;
  }
  client_->SetBeginFrameSource(nullptr);
  support_ = nullptr;
  display_ = nullptr;
  begin_frame_source_ = nullptr;
  parent_local_surface_id_allocator_ = nullptr;
  frame_sink_manager_ = nullptr;
  shared_bitmap_manager_ = nullptr;
  test_client_ = nullptr;
  LayerTreeFrameSink::DetachFromClient();
}

void TestLayerTreeFrameSink::SetLocalSurfaceId(
    const LocalSurfaceId& local_surface_id) {
  test_client_->DisplayReceivedLocalSurfaceId(local_surface_id);
}

void TestLayerTreeFrameSink::SubmitCompositorFrame(CompositorFrame frame,
                                                   bool hit_test_data_changed,
                                                   bool show_hit_test_borders) {
  DCHECK(frame.metadata.begin_frame_ack.has_damage);
  DCHECK_LE(BeginFrameArgs::kStartingFrameNumber,
            frame.metadata.begin_frame_ack.sequence_number);
  test_client_->DisplayReceivedCompositorFrame(frame);

  gfx::Size frame_size = frame.size_in_pixels();
  float device_scale_factor = frame.device_scale_factor();
  LocalSurfaceId local_surface_id =
      parent_local_surface_id_allocator_->GetCurrentLocalSurfaceIdAllocation()
          .local_surface_id();

  if (frame_size != display_size_ ||
      device_scale_factor != device_scale_factor_) {
    parent_local_surface_id_allocator_->GenerateId();
    local_surface_id =
        parent_local_surface_id_allocator_->GetCurrentLocalSurfaceIdAllocation()
            .local_surface_id();
    display_->SetLocalSurfaceId(local_surface_id, device_scale_factor);
    display_->Resize(frame_size);
    display_size_ = frame_size;
    device_scale_factor_ = device_scale_factor;
  }

  support_->SubmitCompositorFrame(local_surface_id, std::move(frame));

  if (!display_->has_scheduler()) {
    display_->DrawAndSwap();
    // Post this to get a new stack frame so that we exit this function before
    // calling the client to tell it that it is done.
    compositor_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&TestLayerTreeFrameSink::SendCompositorFrameAckToClient,
                       weak_ptr_factory_.GetWeakPtr()));
  }
}

void TestLayerTreeFrameSink::DidNotProduceFrame(const BeginFrameAck& ack) {
  DCHECK(!ack.has_damage);
  DCHECK_LE(BeginFrameArgs::kStartingFrameNumber, ack.sequence_number);
  support_->DidNotProduceFrame(ack);
}

void TestLayerTreeFrameSink::DidAllocateSharedBitmap(
    mojo::ScopedSharedBufferHandle buffer,
    const SharedBitmapId& id) {
  bool ok =
      shared_bitmap_manager_->ChildAllocatedSharedBitmap(std::move(buffer), id);
  DCHECK(ok);
  owned_bitmaps_.insert(id);
}

void TestLayerTreeFrameSink::DidDeleteSharedBitmap(const SharedBitmapId& id) {
  shared_bitmap_manager_->ChildDeletedSharedBitmap(id);
  owned_bitmaps_.erase(id);
}

void TestLayerTreeFrameSink::DidReceiveCompositorFrameAck(
    const std::vector<ReturnedResource>& resources) {
  ReclaimResources(resources);
  // In synchronous mode, we manually send acks and this method should not be
  // used.
  if (!display_->has_scheduler())
    return;
  client_->DidReceiveCompositorFrameAck();
}

void TestLayerTreeFrameSink::OnBeginFrame(
    const BeginFrameArgs& args,
    const PresentationFeedbackMap& feedbacks) {
  for (const auto& pair : feedbacks)
    client_->DidPresentCompositorFrame(pair.first, pair.second);
  external_begin_frame_source_.OnBeginFrame(args);
}

void TestLayerTreeFrameSink::ReclaimResources(
    const std::vector<ReturnedResource>& resources) {
  client_->ReclaimResources(resources);
}

void TestLayerTreeFrameSink::OnBeginFramePausedChanged(bool paused) {}

void TestLayerTreeFrameSink::DisplayOutputSurfaceLost() {
  client_->DidLoseLayerTreeFrameSink();
}

void TestLayerTreeFrameSink::DisplayWillDrawAndSwap(
    bool will_draw_and_swap,
    RenderPassList* render_passes) {
  test_client_->DisplayWillDrawAndSwap(will_draw_and_swap, render_passes);
}

void TestLayerTreeFrameSink::DisplayDidDrawAndSwap() {
  test_client_->DisplayDidDrawAndSwap();
}

void TestLayerTreeFrameSink::DisplayDidReceiveCALayerParams(
    const gfx::CALayerParams& ca_layer_params) {}

void TestLayerTreeFrameSink::DisplayDidCompleteSwapWithSize(
    const gfx::Size& pixel_Size) {}

void TestLayerTreeFrameSink::OnNeedsBeginFrames(bool needs_begin_frames) {
  support_->SetNeedsBeginFrame(needs_begin_frames);
}

void TestLayerTreeFrameSink::SendCompositorFrameAckToClient() {
  client_->DidReceiveCompositorFrameAck();
}

base::TimeDelta TestLayerTreeFrameSink::GetPreferredFrameIntervalForFrameSinkId(
    const FrameSinkId& id) {
  return BeginFrameArgs::MinInterval();
}

}  // namespace viz

// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/android/delegated_frame_host_android.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "cc/layers/solid_color_layer.h"
#include "cc/layers/surface_layer.h"
#include "cc/output/compositor_frame.h"
#include "cc/output/copy_output_result.h"
#include "cc/surfaces/local_surface_id_allocator.h"
#include "cc/surfaces/surface.h"
#include "cc/surfaces/surface_id.h"
#include "cc/surfaces/surface_manager.h"
#include "ui/android/view_android.h"
#include "ui/android/window_android_compositor.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/gfx/geometry/dip_util.h"

namespace ui {

namespace {

scoped_refptr<cc::SurfaceLayer> CreateSurfaceLayer(
    cc::SurfaceManager* surface_manager,
    cc::SurfaceId surface_id,
    const gfx::Size surface_size,
    bool surface_opaque) {
  // manager must outlive compositors using it.
  auto layer = cc::SurfaceLayer::Create(surface_manager->reference_factory());
  layer->SetPrimarySurfaceInfo(cc::SurfaceInfo(surface_id, 1.f, surface_size));
  layer->SetBounds(surface_size);
  layer->SetIsDrawable(true);
  layer->SetContentsOpaque(surface_opaque);

  return layer;
}

void CopyOutputRequestCallback(
    scoped_refptr<cc::Layer> readback_layer,
    cc::CopyOutputRequest::CopyOutputRequestCallback result_callback,
    std::unique_ptr<cc::CopyOutputResult> copy_output_result) {
  readback_layer->RemoveFromParent();
  result_callback.Run(std::move(copy_output_result));
}

}  // namespace

DelegatedFrameHostAndroid::DelegatedFrameHostAndroid(
    ui::ViewAndroid* view,
    cc::SurfaceManager* surface_manager,
    Client* client,
    const cc::FrameSinkId& frame_sink_id)
    : frame_sink_id_(frame_sink_id),
      view_(view),
      surface_manager_(surface_manager),
      client_(client),
      begin_frame_source_(this) {
  DCHECK(view_);
  DCHECK(client_);

  local_surface_id_allocator_.reset(new cc::LocalSurfaceIdAllocator());
  surface_manager_->RegisterFrameSinkId(frame_sink_id_);
  CreateNewCompositorFrameSinkSupport();
}

DelegatedFrameHostAndroid::~DelegatedFrameHostAndroid() {
  DestroyDelegatedContent();
  DetachFromCompositor();
  support_.reset();
  surface_manager_->InvalidateFrameSinkId(frame_sink_id_);
}

DelegatedFrameHostAndroid::FrameData::FrameData() = default;

DelegatedFrameHostAndroid::FrameData::~FrameData() = default;

void DelegatedFrameHostAndroid::SubmitCompositorFrame(
    cc::CompositorFrame frame) {
  cc::RenderPass* root_pass = frame.render_pass_list.back().get();
  gfx::Size surface_size = root_pass->output_rect.size();

  if (!current_frame_ || surface_size != current_frame_->surface_size ||
      current_frame_->top_controls_height !=
          frame.metadata.top_controls_height ||
      current_frame_->top_controls_shown_ratio !=
          frame.metadata.top_controls_shown_ratio ||
      current_frame_->bottom_controls_height !=
          frame.metadata.bottom_controls_height ||
      current_frame_->bottom_controls_shown_ratio !=
          frame.metadata.bottom_controls_shown_ratio ||
      current_frame_->viewport_selection != frame.metadata.selection ||
      current_frame_->has_transparent_background !=
          root_pass->has_transparent_background) {
    DestroyDelegatedContent();
    DCHECK(!content_layer_);
    DCHECK(!current_frame_);

    current_frame_ = base::MakeUnique<FrameData>();
    current_frame_->local_surface_id =
        local_surface_id_allocator_->GenerateId();
    current_frame_->surface_size = surface_size;
    current_frame_->top_controls_height = frame.metadata.top_controls_height;
    current_frame_->top_controls_shown_ratio =
        frame.metadata.top_controls_shown_ratio;
    current_frame_->bottom_controls_height =
        frame.metadata.bottom_controls_height;
    current_frame_->bottom_controls_shown_ratio =
        frame.metadata.bottom_controls_shown_ratio;
    current_frame_->has_transparent_background =
        root_pass->has_transparent_background;
    current_frame_->viewport_selection = frame.metadata.selection;
    support_->SubmitCompositorFrame(current_frame_->local_surface_id,
                                    std::move(frame));

    content_layer_ = CreateSurfaceLayer(
        surface_manager_,
        cc::SurfaceId(frame_sink_id_, current_frame_->local_surface_id),
        current_frame_->surface_size,
        !current_frame_->has_transparent_background);
    view_->GetLayer()->AddChild(content_layer_);
  } else {
    support_->SubmitCompositorFrame(current_frame_->local_surface_id,
                                    std::move(frame));
  }
}

cc::FrameSinkId DelegatedFrameHostAndroid::GetFrameSinkId() const {
  return frame_sink_id_;
}

void DelegatedFrameHostAndroid::RequestCopyOfSurface(
    WindowAndroidCompositor* compositor,
    const gfx::Rect& src_subrect_in_pixel,
    cc::CopyOutputRequest::CopyOutputRequestCallback result_callback) {
  DCHECK(current_frame_);
  DCHECK(!result_callback.is_null());

  scoped_refptr<cc::Layer> readback_layer = CreateSurfaceLayer(
      surface_manager_,
      cc::SurfaceId(frame_sink_id_, current_frame_->local_surface_id),
      current_frame_->surface_size,
      !current_frame_->has_transparent_background);
  readback_layer->SetHideLayerAndSubtree(true);
  compositor->AttachLayerForReadback(readback_layer);
  std::unique_ptr<cc::CopyOutputRequest> copy_output_request =
      cc::CopyOutputRequest::CreateRequest(base::Bind(
          &CopyOutputRequestCallback, readback_layer, result_callback));

  if (!src_subrect_in_pixel.IsEmpty())
    copy_output_request->set_area(src_subrect_in_pixel);

  support_->RequestCopyOfSurface(std::move(copy_output_request));
}

void DelegatedFrameHostAndroid::DestroyDelegatedContent() {
  if (!current_frame_)
    return;

  DCHECK(content_layer_);

  content_layer_->RemoveFromParent();
  content_layer_ = nullptr;
  support_->EvictFrame();
  current_frame_.reset();
}

bool DelegatedFrameHostAndroid::HasDelegatedContent() const {
  return current_frame_.get() != nullptr;
}

void DelegatedFrameHostAndroid::CompositorFrameSinkChanged() {
  DestroyDelegatedContent();
  CreateNewCompositorFrameSinkSupport();
  if (registered_parent_compositor_)
    AttachToCompositor(registered_parent_compositor_);
}

void DelegatedFrameHostAndroid::AttachToCompositor(
    WindowAndroidCompositor* compositor) {
  if (registered_parent_compositor_)
    DetachFromCompositor();
  compositor->AddChildFrameSink(frame_sink_id_);
  client_->SetBeginFrameSource(&begin_frame_source_);
  registered_parent_compositor_ = compositor;
}

void DelegatedFrameHostAndroid::DetachFromCompositor() {
  if (!registered_parent_compositor_)
    return;
  client_->SetBeginFrameSource(nullptr);
  support_->SetNeedsBeginFrame(false);
  registered_parent_compositor_->RemoveChildFrameSink(frame_sink_id_);
  registered_parent_compositor_ = nullptr;
}

void DelegatedFrameHostAndroid::DidReceiveCompositorFrameAck() {
  client_->DidReceiveCompositorFrameAck();
}

void DelegatedFrameHostAndroid::OnBeginFrame(const cc::BeginFrameArgs& args) {
  begin_frame_source_.OnBeginFrame(args);
}

void DelegatedFrameHostAndroid::ReclaimResources(
    const cc::ReturnedResourceArray& resources) {
  client_->ReclaimResources(resources);
}

void DelegatedFrameHostAndroid::WillDrawSurface(
    const cc::LocalSurfaceId& local_surface_id,
    const gfx::Rect& damage_rect) {}

void DelegatedFrameHostAndroid::OnNeedsBeginFrames(bool needs_begin_frames) {
  support_->SetNeedsBeginFrame(needs_begin_frames);
}

void DelegatedFrameHostAndroid::OnDidFinishFrame(const cc::BeginFrameAck& ack) {
}

void DelegatedFrameHostAndroid::CreateNewCompositorFrameSinkSupport() {
  support_.reset();
  support_ = base::MakeUnique<cc::CompositorFrameSinkSupport>(
      this, surface_manager_, frame_sink_id_, false /* is_root */,
      false /* handles_frame_sink_id_invalidation */,
      true /* needs_sync_points */);
}

}  // namespace ui

// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/android/delegated_frame_host_android.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "cc/layers/solid_color_layer.h"
#include "cc/layers/surface_layer.h"
#include "components/viz/common/frame_sinks/copy_output_result.h"
#include "components/viz/common/quads/compositor_frame.h"
#include "components/viz/common/surfaces/surface_id.h"
#include "components/viz/host/host_frame_sink_manager.h"
#include "components/viz/service/frame_sinks/frame_sink_manager_impl.h"
#include "components/viz/service/surfaces/surface.h"
#include "ui/android/view_android.h"
#include "ui/android/window_android_compositor.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/gfx/geometry/dip_util.h"

namespace ui {

namespace {

scoped_refptr<cc::SurfaceLayer> CreateSurfaceLayer(
    viz::SurfaceManager* surface_manager,
    viz::SurfaceInfo surface_info,
    bool surface_opaque) {
  // manager must outlive compositors using it.
  auto layer = cc::SurfaceLayer::Create(surface_manager->reference_factory());
  layer->SetPrimarySurfaceInfo(surface_info);
  layer->SetFallbackSurfaceInfo(surface_info);
  layer->SetBounds(surface_info.size_in_pixels());
  layer->SetIsDrawable(true);
  layer->SetContentsOpaque(surface_opaque);

  return layer;
}

void CopyOutputRequestCallback(
    scoped_refptr<cc::Layer> readback_layer,
    viz::CopyOutputRequest::CopyOutputRequestCallback result_callback,
    std::unique_ptr<viz::CopyOutputResult> copy_output_result) {
  readback_layer->RemoveFromParent();
  std::move(result_callback).Run(std::move(copy_output_result));
}

}  // namespace

DelegatedFrameHostAndroid::DelegatedFrameHostAndroid(
    ui::ViewAndroid* view,
    viz::HostFrameSinkManager* host_frame_sink_manager,
    viz::FrameSinkManagerImpl* frame_sink_manager,
    Client* client,
    const viz::FrameSinkId& frame_sink_id)
    : frame_sink_id_(frame_sink_id),
      view_(view),
      host_frame_sink_manager_(host_frame_sink_manager),
      frame_sink_manager_(frame_sink_manager),
      client_(client),
      begin_frame_source_(this) {
  DCHECK(view_);
  DCHECK(client_);

  host_frame_sink_manager_->RegisterFrameSinkId(frame_sink_id_, this);
#if DCHECK_IS_ON()
  host_frame_sink_manager_->SetFrameSinkDebugLabel(frame_sink_id_,
                                                   "DelegatedFrameHostAndroid");
#endif
  CreateNewCompositorFrameSinkSupport();
}

DelegatedFrameHostAndroid::~DelegatedFrameHostAndroid() {
  DestroyDelegatedContent();
  DetachFromCompositor();
  support_.reset();
  host_frame_sink_manager_->InvalidateFrameSinkId(frame_sink_id_);
}

void DelegatedFrameHostAndroid::SubmitCompositorFrame(
    const viz::LocalSurfaceId& local_surface_id,
    viz::CompositorFrame frame) {
  if (local_surface_id != surface_info_.id().local_surface_id()) {
    DestroyDelegatedContent();
    DCHECK(!content_layer_);

    viz::RenderPass* root_pass = frame.render_pass_list.back().get();
    gfx::Size frame_size = root_pass->output_rect.size();
    surface_info_ = viz::SurfaceInfo(
        viz::SurfaceId(frame_sink_id_, local_surface_id), 1.f, frame_size);
    has_transparent_background_ = root_pass->has_transparent_background;

    bool result =
        support_->SubmitCompositorFrame(local_surface_id, std::move(frame));
    DCHECK(result);

    content_layer_ =
        CreateSurfaceLayer(frame_sink_manager_->surface_manager(),
                           surface_info_, !has_transparent_background_);
    view_->GetLayer()->AddChild(content_layer_);
  } else {
    support_->SubmitCompositorFrame(local_surface_id, std::move(frame));
  }
}

void DelegatedFrameHostAndroid::DidNotProduceFrame(
    const viz::BeginFrameAck& ack) {
  support_->DidNotProduceFrame(ack);
}

viz::FrameSinkId DelegatedFrameHostAndroid::GetFrameSinkId() const {
  return frame_sink_id_;
}

void DelegatedFrameHostAndroid::RequestCopyOfSurface(
    WindowAndroidCompositor* compositor,
    const gfx::Rect& src_subrect_in_pixel,
    viz::CopyOutputRequest::CopyOutputRequestCallback result_callback) {
  DCHECK(surface_info_.is_valid());
  DCHECK(!result_callback.is_null());

  scoped_refptr<cc::Layer> readback_layer =
      CreateSurfaceLayer(frame_sink_manager_->surface_manager(), surface_info_,
                         !has_transparent_background_);
  readback_layer->SetHideLayerAndSubtree(true);
  compositor->AttachLayerForReadback(readback_layer);
  std::unique_ptr<viz::CopyOutputRequest> copy_output_request =
      std::make_unique<viz::CopyOutputRequest>(
          viz::CopyOutputRequest::ResultFormat::RGBA_TEXTURE,
          base::BindOnce(&CopyOutputRequestCallback, readback_layer,
                         std::move(result_callback)));

  if (!src_subrect_in_pixel.IsEmpty())
    copy_output_request->set_area(src_subrect_in_pixel);

  support_->RequestCopyOfSurface(std::move(copy_output_request));
}

void DelegatedFrameHostAndroid::DestroyDelegatedContent() {
  if (!surface_info_.is_valid())
    return;

  DCHECK(content_layer_);

  content_layer_->RemoveFromParent();
  content_layer_ = nullptr;
  support_->EvictCurrentSurface();
  surface_info_ = viz::SurfaceInfo();
}

bool DelegatedFrameHostAndroid::HasDelegatedContent() const {
  return surface_info_.is_valid();
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

void DelegatedFrameHostAndroid::DidReceiveCompositorFrameAck(
    const std::vector<viz::ReturnedResource>& resources) {
  client_->ReclaimResources(resources);
  client_->DidReceiveCompositorFrameAck();
}

void DelegatedFrameHostAndroid::OnBeginFrame(const viz::BeginFrameArgs& args) {
  begin_frame_source_.OnBeginFrame(args);
}

void DelegatedFrameHostAndroid::ReclaimResources(
    const std::vector<viz::ReturnedResource>& resources) {
  client_->ReclaimResources(resources);
}

void DelegatedFrameHostAndroid::OnBeginFramePausedChanged(bool paused) {
  begin_frame_source_.OnSetBeginFrameSourcePaused(paused);
}

void DelegatedFrameHostAndroid::OnNeedsBeginFrames(bool needs_begin_frames) {
  support_->SetNeedsBeginFrame(needs_begin_frames);
}

void DelegatedFrameHostAndroid::OnFirstSurfaceActivation(
    const viz::SurfaceInfo& surface_info) {
  // TODO(fsamuel): Once surface synchronization is turned on, the fallback
  // surface should be set here.
}

void DelegatedFrameHostAndroid::CreateNewCompositorFrameSinkSupport() {
  constexpr bool is_root = false;
  constexpr bool needs_sync_points = true;
  support_.reset();
  support_ = host_frame_sink_manager_->CreateCompositorFrameSinkSupport(
      this, frame_sink_id_, is_root, needs_sync_points);
}

viz::SurfaceId DelegatedFrameHostAndroid::SurfaceId() const {
  return surface_info_.id();
}

}  // namespace ui

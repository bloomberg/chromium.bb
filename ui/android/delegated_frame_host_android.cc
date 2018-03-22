// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/android/delegated_frame_host_android.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "cc/layers/solid_color_layer.h"
#include "cc/layers/surface_layer.h"
#include "components/viz/common/features.h"
#include "components/viz/common/frame_sinks/copy_output_result.h"
#include "components/viz/common/quads/compositor_frame.h"
#include "components/viz/common/surfaces/surface_id.h"
#include "components/viz/host/host_frame_sink_manager.h"
#include "components/viz/service/surfaces/surface.h"
#include "ui/android/view_android.h"
#include "ui/android/window_android.h"
#include "ui/android/window_android_compositor.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/gfx/geometry/dip_util.h"

namespace ui {

namespace {

scoped_refptr<cc::SurfaceLayer> CreateSurfaceLayer(
    viz::SurfaceInfo surface_info,
    bool surface_opaque) {
  // manager must outlive compositors using it.
  auto layer = cc::SurfaceLayer::Create();
  layer->SetPrimarySurfaceId(surface_info.id(),
                             cc::DeadlinePolicy::UseDefaultDeadline());
  layer->SetFallbackSurfaceId(surface_info.id());
  layer->SetBounds(surface_info.size_in_pixels());
  layer->SetIsDrawable(true);
  layer->SetContentsOpaque(surface_opaque);
  layer->SetHitTestable(true);

  return layer;
}

}  // namespace

DelegatedFrameHostAndroid::DelegatedFrameHostAndroid(
    ui::ViewAndroid* view,
    viz::HostFrameSinkManager* host_frame_sink_manager,
    Client* client,
    const viz::FrameSinkId& frame_sink_id)
    : frame_sink_id_(frame_sink_id),
      view_(view),
      host_frame_sink_manager_(host_frame_sink_manager),
      client_(client),
      begin_frame_source_(this),
      enable_surface_synchronization_(
          features::IsSurfaceSynchronizationEnabled()) {
  DCHECK(view_);
  DCHECK(client_);

  host_frame_sink_manager_->RegisterFrameSinkId(frame_sink_id_, this);
  host_frame_sink_manager_->SetFrameSinkDebugLabel(frame_sink_id_,
                                                   "DelegatedFrameHostAndroid");
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
    viz::CompositorFrame frame,
    viz::mojom::HitTestRegionListPtr hit_test_region_list) {
  if (local_surface_id != surface_info_.id().local_surface_id()) {
    DestroyDelegatedContent();
    DCHECK(!content_layer_);

    viz::RenderPass* root_pass = frame.render_pass_list.back().get();
    gfx::Size frame_size = root_pass->output_rect.size();
    surface_info_ = viz::SurfaceInfo(
        viz::SurfaceId(frame_sink_id_, local_surface_id), 1.f, frame_size);
    has_transparent_background_ = root_pass->has_transparent_background;

    support_->SubmitCompositorFrame(local_surface_id, std::move(frame),
                                    std::move(hit_test_region_list));

    content_layer_ =
        CreateSurfaceLayer(surface_info_, !has_transparent_background_);
    view_->GetLayer()->AddChild(content_layer_);
  } else {
    support_->SubmitCompositorFrame(local_surface_id, std::move(frame),
                                    std::move(hit_test_region_list));
  }
  compositor_attach_until_frame_lock_.reset();
}

void DelegatedFrameHostAndroid::DidNotProduceFrame(
    const viz::BeginFrameAck& ack) {
  support_->DidNotProduceFrame(ack);
}

viz::FrameSinkId DelegatedFrameHostAndroid::GetFrameSinkId() const {
  return frame_sink_id_;
}

void DelegatedFrameHostAndroid::CopyFromCompositingSurface(
    const gfx::Rect& src_subrect,
    const gfx::Size& output_size,
    base::OnceCallback<void(const SkBitmap&)> callback) {
  if (!CanCopyFromCompositingSurface()) {
    std::move(callback).Run(SkBitmap());
    return;
  }

  scoped_refptr<cc::Layer> readback_layer =
      CreateSurfaceLayer(surface_info_, !has_transparent_background_);
  readback_layer->SetHideLayerAndSubtree(true);
  view_->GetWindowAndroid()->GetCompositor()->AttachLayerForReadback(
      readback_layer);
  std::unique_ptr<viz::CopyOutputRequest> request =
      std::make_unique<viz::CopyOutputRequest>(
          viz::CopyOutputRequest::ResultFormat::RGBA_BITMAP,
          base::BindOnce(
              [](base::OnceCallback<void(const SkBitmap&)> callback,
                 scoped_refptr<cc::Layer> readback_layer,
                 std::unique_ptr<viz::CopyOutputResult> result) {
                readback_layer->RemoveFromParent();
                std::move(callback).Run(result->AsSkBitmap());
              },
              std::move(callback), std::move(readback_layer)));

  if (src_subrect.IsEmpty()) {
    request->set_area(gfx::Rect(surface_info_.size_in_pixels()));
  } else {
    request->set_area(
        gfx::ConvertRectToPixel(view_->GetDipScale(), src_subrect));
  }

  if (!output_size.IsEmpty()) {
    request->set_result_selection(gfx::Rect(output_size));
    request->SetScaleRatio(
        gfx::Vector2d(request->area().width(), request->area().height()),
        gfx::Vector2d(output_size.width(), output_size.height()));
  }

  support_->RequestCopyOfOutput(std::move(request));
}

bool DelegatedFrameHostAndroid::CanCopyFromCompositingSurface() const {
  return support_ && surface_info_.is_valid() && view_->GetWindowAndroid() &&
         view_->GetWindowAndroid()->GetCompositor();
}

void DelegatedFrameHostAndroid::DestroyDelegatedContent() {
  if (!surface_info_.is_valid())
    return;

  DCHECK(content_layer_);

  content_layer_->RemoveFromParent();
  content_layer_ = nullptr;
  support_->EvictLastActivatedSurface();
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
  // If this is the first frame since the compositor became visible, we want to
  // take the compositor lock, preventing compositor frames from being produced
  // until all delegated frames are ready. This improves the resume transition,
  // preventing flashes. Set a 5 second timeout to prevent locking up the
  // browser in cases where the renderer hangs or another factor prevents a
  // frame from being produced. If we already have delegated content, no need
  // to take the lock.
  if (compositor->IsDrawingFirstVisibleFrame() && !HasDelegatedContent()) {
    compositor_attach_until_frame_lock_ =
        compositor->GetCompositorLock(this, base::TimeDelta::FromSeconds(5));
  }
  compositor->AddChildFrameSink(frame_sink_id_);
  client_->SetBeginFrameSource(&begin_frame_source_);
  registered_parent_compositor_ = compositor;
}

void DelegatedFrameHostAndroid::DetachFromCompositor() {
  if (!registered_parent_compositor_)
    return;
  compositor_attach_until_frame_lock_.reset();
  client_->SetBeginFrameSource(nullptr);
  support_->SetNeedsBeginFrame(false);
  registered_parent_compositor_->RemoveChildFrameSink(frame_sink_id_);
  registered_parent_compositor_ = nullptr;
}

void DelegatedFrameHostAndroid::WasResized() {
  if (enable_surface_synchronization_) {
    local_surface_id_ = local_surface_id_allocator_.GenerateId();

    // TODO(ericrk): We should handle updating our surface layer with the new
    // ID. See similar behavior in delegated_frame_host.cc.
    // https://crbug.com/801350
  }
}

void DelegatedFrameHostAndroid::DidReceiveCompositorFrameAck(
    const std::vector<viz::ReturnedResource>& resources) {
  client_->ReclaimResources(resources);
  client_->DidReceiveCompositorFrameAck();
}

void DelegatedFrameHostAndroid::DidPresentCompositorFrame(
    uint32_t presentation_token,
    base::TimeTicks time,
    base::TimeDelta refresh,
    uint32_t flags) {
  client_->DidPresentCompositorFrame(presentation_token, time, refresh, flags);
}

void DelegatedFrameHostAndroid::DidDiscardCompositorFrame(
    uint32_t presentation_token) {
  client_->DidDiscardCompositorFrame(presentation_token);
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

void DelegatedFrameHostAndroid::OnFrameTokenChanged(uint32_t frame_token) {
  client_->OnFrameTokenChanged(frame_token);
}

void DelegatedFrameHostAndroid::CompositorLockTimedOut() {}

void DelegatedFrameHostAndroid::CreateNewCompositorFrameSinkSupport() {
  constexpr bool is_root = false;
  constexpr bool needs_sync_points = true;
  support_.reset();
  support_ = host_frame_sink_manager_->CreateCompositorFrameSinkSupport(
      this, frame_sink_id_, is_root, needs_sync_points);
}

const viz::SurfaceId& DelegatedFrameHostAndroid::SurfaceId() const {
  return surface_info_.id();
}

const viz::LocalSurfaceId& DelegatedFrameHostAndroid::GetLocalSurfaceId()
    const {
  return local_surface_id_;
}

}  // namespace ui

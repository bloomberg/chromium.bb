// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ui/ws/frame_generator.h"

#include "base/containers/adapters.h"
#include "cc/output/compositor_frame.h"
#include "cc/quads/render_pass.h"
#include "cc/quads/render_pass_draw_quad.h"
#include "cc/quads/shared_quad_state.h"
#include "cc/quads/surface_draw_quad.h"
#include "cc/surfaces/surface_id.h"
#include "gpu/ipc/client/gpu_channel_host.h"
#include "services/ui/surfaces/display_compositor_frame_sink.h"
#include "services/ui/ws/frame_generator_delegate.h"
#include "services/ui/ws/server_window.h"
#include "services/ui/ws/server_window_compositor_frame_sink.h"
#include "services/ui/ws/server_window_compositor_frame_sink_manager.h"

namespace ui {

namespace ws {

FrameGenerator::FrameGenerator(
    FrameGeneratorDelegate* delegate,
    ServerWindow* root_window,
    scoped_refptr<DisplayCompositor> display_compositor)
    : delegate_(delegate),
      display_compositor_(display_compositor),
      frame_sink_id_(
          WindowIdToTransportId(root_window->id()),
          static_cast<uint32_t>(mojom::CompositorFrameSinkType::DEFAULT)),
      draw_timer_(false, false),
      weak_factory_(this) {
  DCHECK(delegate_);
  surface_sequence_generator_.set_frame_sink_id(frame_sink_id_);
}

FrameGenerator::~FrameGenerator() {
  ReleaseAllSurfaceReferences();
  // Invalidate WeakPtrs now to avoid callbacks back into the
  // FrameGenerator during destruction of |compositor_frame_sink_|.
  weak_factory_.InvalidateWeakPtrs();
  compositor_frame_sink_.reset();
}

void FrameGenerator::OnGpuChannelEstablished(
    scoped_refptr<gpu::GpuChannelHost> channel) {
  if (widget_ != gfx::kNullAcceleratedWidget) {
    compositor_frame_sink_ = base::MakeUnique<DisplayCompositorFrameSink>(
        frame_sink_id_, base::ThreadTaskRunnerHandle::Get(), widget_,
        std::move(channel), display_compositor_);
  } else {
    gpu_channel_ = std::move(channel);
  }
}

void FrameGenerator::RequestRedraw(const gfx::Rect& redraw_region) {
  dirty_rect_.Union(redraw_region);
  WantToDraw();
}

void FrameGenerator::OnAcceleratedWidgetAvailable(
    gfx::AcceleratedWidget widget) {
  widget_ = widget;
  if (gpu_channel_ && widget != gfx::kNullAcceleratedWidget) {
    compositor_frame_sink_ = base::MakeUnique<DisplayCompositorFrameSink>(
        frame_sink_id_, base::ThreadTaskRunnerHandle::Get(), widget_,
        std::move(gpu_channel_), display_compositor_);
  }
}

void FrameGenerator::WantToDraw() {
  if (draw_timer_.IsRunning() || frame_pending_)
    return;

  // TODO(rjkroege): Use vblank to kick off Draw.
  draw_timer_.Start(
      FROM_HERE, base::TimeDelta(),
      base::Bind(&FrameGenerator::Draw, weak_factory_.GetWeakPtr()));
}

void FrameGenerator::Draw() {
  if (!delegate_->GetRootWindow()->visible())
    return;

  const gfx::Rect output_rect(delegate_->GetViewportMetrics().pixel_size);
  dirty_rect_.Intersect(output_rect);
  // TODO(fsamuel): We should add a trace for generating a top level frame.
  cc::CompositorFrame frame(GenerateCompositorFrame(output_rect));
  if (compositor_frame_sink_) {
    frame_pending_ = true;
    compositor_frame_sink_->SubmitCompositorFrame(
        std::move(frame),
        base::Bind(&FrameGenerator::DidDraw, weak_factory_.GetWeakPtr()));
  }
  dirty_rect_ = gfx::Rect();
}

void FrameGenerator::DidDraw() {
  frame_pending_ = false;
  if (!dirty_rect_.IsEmpty())
    WantToDraw();
}

cc::CompositorFrame FrameGenerator::GenerateCompositorFrame(
    const gfx::Rect& output_rect) {
  const cc::RenderPassId render_pass_id(1, 1);
  std::unique_ptr<cc::RenderPass> render_pass = cc::RenderPass::Create();
  render_pass->SetNew(render_pass_id, output_rect, dirty_rect_,
                      gfx::Transform());

  DrawWindowTree(render_pass.get(), delegate_->GetRootWindow(), gfx::Vector2d(),
                 1.0f);

  std::unique_ptr<cc::DelegatedFrameData> frame_data(
      new cc::DelegatedFrameData);
  frame_data->render_pass_list.push_back(std::move(render_pass));
  if (delegate_->IsInHighContrastMode()) {
    std::unique_ptr<cc::RenderPass> invert_pass = cc::RenderPass::Create();
    invert_pass->SetNew(cc::RenderPassId(2, 0), output_rect, dirty_rect_,
                        gfx::Transform());
    cc::SharedQuadState* shared_state =
        invert_pass->CreateAndAppendSharedQuadState();
    shared_state->SetAll(gfx::Transform(), output_rect.size(), output_rect,
                         output_rect, false, 1.f, SkXfermode::kSrcOver_Mode, 0);
    auto* quad = invert_pass->CreateAndAppendDrawQuad<cc::RenderPassDrawQuad>();
    cc::FilterOperations filters;
    filters.Append(cc::FilterOperation::CreateInvertFilter(1.f));
    quad->SetNew(shared_state, output_rect, output_rect, render_pass_id,
                 0 /* mask_resource_id */, gfx::Vector2dF() /* mask_uv_scale */,
                 gfx::Size() /* mask_texture_size */, filters,
                 gfx::Vector2dF() /* filters_scale */,
                 gfx::PointF() /* filters_origin */,
                 cc::FilterOperations() /* background_filters */);
    frame_data->render_pass_list.push_back(std::move(invert_pass));
  }

  cc::CompositorFrame frame;
  frame.delegated_frame_data = std::move(frame_data);
  return frame;
}

void FrameGenerator::DrawWindowTree(
    cc::RenderPass* pass,
    ServerWindow* window,
    const gfx::Vector2d& parent_to_root_origin_offset,
    float opacity) {
  if (!window->visible())
    return;

  const gfx::Rect absolute_bounds =
      window->bounds() + parent_to_root_origin_offset;
  const ServerWindow::Windows& children = window->children();
  const float combined_opacity = opacity * window->opacity();
  for (ServerWindow* child : base::Reversed(children)) {
    DrawWindowTree(pass, child, absolute_bounds.OffsetFromOrigin(),
                   combined_opacity);
  }

  if (!window->compositor_frame_sink_manager() ||
      !window->compositor_frame_sink_manager()->ShouldDraw())
    return;

  cc::SurfaceId underlay_surface_id =
      window->compositor_frame_sink_manager()->GetLatestSurfaceId(
          mojom::CompositorFrameSinkType::UNDERLAY);
  cc::SurfaceId default_surface_id =
      window->compositor_frame_sink_manager()->GetLatestSurfaceId(
          mojom::CompositorFrameSinkType::DEFAULT);

  if (underlay_surface_id.is_null() && default_surface_id.is_null())
    return;

  if (!default_surface_id.is_null()) {
    gfx::Transform quad_to_target_transform;
    quad_to_target_transform.Translate(absolute_bounds.x(),
                                       absolute_bounds.y());

    cc::SharedQuadState* sqs = pass->CreateAndAppendSharedQuadState();

    const gfx::Rect bounds_at_origin(window->bounds().size());
    // TODO(fsamuel): These clipping and visible rects are incorrect. They need
    // to be populated from CompositorFrame structs.
    sqs->SetAll(quad_to_target_transform,
                bounds_at_origin.size() /* layer_bounds */,
                bounds_at_origin /* visible_layer_bounds */,
                bounds_at_origin /* clip_rect */, false /* is_clipped */,
                combined_opacity, SkXfermode::kSrcOver_Mode,
                0 /* sorting-context_id */);
    auto* quad = pass->CreateAndAppendDrawQuad<cc::SurfaceDrawQuad>();
    AddOrUpdateSurfaceReference(mojom::CompositorFrameSinkType::DEFAULT,
                                window);
    quad->SetAll(sqs, bounds_at_origin /* rect */,
                 gfx::Rect() /* opaque_rect */,
                 bounds_at_origin /* visible_rect */, true /* needs_blending*/,
                 default_surface_id);
  }
  if (!underlay_surface_id.is_null()) {
    const gfx::Rect underlay_absolute_bounds =
        absolute_bounds - window->underlay_offset();
    gfx::Transform quad_to_target_transform;
    quad_to_target_transform.Translate(underlay_absolute_bounds.x(),
                                       underlay_absolute_bounds.y());
    cc::SharedQuadState* sqs = pass->CreateAndAppendSharedQuadState();
    const gfx::Rect bounds_at_origin(
        window->compositor_frame_sink_manager()->GetLatestFrameSize(
            mojom::CompositorFrameSinkType::UNDERLAY));
    sqs->SetAll(quad_to_target_transform,
                bounds_at_origin.size() /* layer_bounds */,
                bounds_at_origin /* visible_layer_bounds */,
                bounds_at_origin /* clip_rect */, false /* is_clipped */,
                combined_opacity, SkXfermode::kSrcOver_Mode,
                0 /* sorting-context_id */);

    auto* quad = pass->CreateAndAppendDrawQuad<cc::SurfaceDrawQuad>();
    AddOrUpdateSurfaceReference(mojom::CompositorFrameSinkType::UNDERLAY,
                                window);
    quad->SetAll(sqs, bounds_at_origin /* rect */,
                 gfx::Rect() /* opaque_rect */,
                 bounds_at_origin /* visible_rect */, true /* needs_blending*/,
                 underlay_surface_id);
  }
}

void FrameGenerator::AddOrUpdateSurfaceReference(
    mojom::CompositorFrameSinkType type,
    ServerWindow* window) {
  cc::SurfaceId surface_id =
      window->compositor_frame_sink_manager()->GetLatestSurfaceId(type);
  if (surface_id.is_null())
    return;
  // TODO(fsamuel): Use mojo interface to give root window a surface reference.
  cc::SurfaceManager* surface_manager = display_compositor_->manager();
  auto it = dependencies_.find(surface_id.frame_sink_id());
  if (it == dependencies_.end()) {
    cc::Surface* surface = surface_manager->GetSurfaceForId(surface_id);
    if (!surface) {
      LOG(ERROR) << "Attempting to add dependency to nonexistent surface "
                 << surface_id.ToString();
      return;
    }
    SurfaceDependency dependency = {
        surface_id.local_frame_id(),
        surface_sequence_generator_.CreateSurfaceSequence()};
    surface->AddDestructionDependency(dependency.sequence);
    dependencies_[surface_id.frame_sink_id()] = dependency;
    // Observe |window_surface|'s window so that we can release references when
    // the window is destroyed.
    Add(window);
    return;
  }

  // We are already holding a reference to this surface so there's no work to do
  // here.
  if (surface_id.local_frame_id() == it->second.local_frame_id)
    return;

  // If we have have an existing reference to a surface from the given
  // FrameSink, then we should release the reference, and then add this new
  // reference. This results in a delete and lookup in the map but simplifies
  // the code.
  ReleaseFrameSinkReference(surface_id.frame_sink_id());

  // This recursion will always terminate. This line is being called because
  // there was a stale surface reference. The stale reference has been released
  // in the previous line and cleared from the dependencies_ map. Thus, in the
  // recursive call, we'll enter the second if blcok because the FrameSinkId
  // is no longer referenced in the map.
  AddOrUpdateSurfaceReference(type, window);
}

void FrameGenerator::ReleaseFrameSinkReference(
    const cc::FrameSinkId& frame_sink_id) {
  auto it = dependencies_.find(frame_sink_id);
  if (it == dependencies_.end())
    return;
  std::vector<uint32_t> sequences;
  sequences.push_back(it->second.sequence.sequence);
  cc::SurfaceManager* surface_manager = display_compositor_->manager();
  surface_manager->DidSatisfySequences(frame_sink_id_, &sequences);
  dependencies_.erase(it);
}

void FrameGenerator::ReleaseAllSurfaceReferences() {
  cc::SurfaceManager* surface_manager = display_compositor_->manager();
  std::vector<uint32_t> sequences;
  for (auto& dependency : dependencies_)
    sequences.push_back(dependency.second.sequence.sequence);
  surface_manager->DidSatisfySequences(frame_sink_id_, &sequences);
  dependencies_.clear();
}

void FrameGenerator::OnWindowDestroying(ServerWindow* window) {
  Remove(window);
  ServerWindowCompositorFrameSinkManager* compositor_frame_sink_manager =
      window->compositor_frame_sink_manager();
  // If FrameGenerator was observing |window|, then that means it had a
  // CompositorFrame at some point in time and should have a
  // ServerWindowCompositorFrameSinkManager.
  DCHECK(compositor_frame_sink_manager);

  cc::SurfaceId default_surface_id =
      window->compositor_frame_sink_manager()->GetLatestSurfaceId(
          mojom::CompositorFrameSinkType::DEFAULT);
  if (!default_surface_id.is_null())
    ReleaseFrameSinkReference(default_surface_id.frame_sink_id());

  cc::SurfaceId underlay_surface_id =
      window->compositor_frame_sink_manager()->GetLatestSurfaceId(
          mojom::CompositorFrameSinkType::UNDERLAY);
  if (!underlay_surface_id.is_null())
    ReleaseFrameSinkReference(underlay_surface_id.frame_sink_id());
}

}  // namespace ws

}  // namespace ui

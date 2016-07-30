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
#include "services/ui/surfaces/display_compositor.h"
#include "services/ui/ws/frame_generator_delegate.h"
#include "services/ui/ws/server_window.h"
#include "services/ui/ws/server_window_surface.h"
#include "services/ui/ws/server_window_surface_manager.h"

namespace ui {

namespace ws {

FrameGenerator::FrameGenerator(FrameGeneratorDelegate* delegate,
                               scoped_refptr<SurfacesState> surfaces_state)
    : delegate_(delegate),
      surfaces_state_(surfaces_state),
      draw_timer_(false, false),
      weak_factory_(this) {
  DCHECK(delegate_);
}

FrameGenerator::~FrameGenerator() {
  // Invalidate WeakPtrs now to avoid callbacks back into the
  // FrameGenerator during destruction of |display_compositor_|.
  weak_factory_.InvalidateWeakPtrs();
  display_compositor_.reset();
}

void FrameGenerator::RequestRedraw(const gfx::Rect& redraw_region) {
  dirty_rect_.Union(redraw_region);
  WantToDraw();
}

void FrameGenerator::OnAcceleratedWidgetAvailable(
    gfx::AcceleratedWidget widget) {
  if (widget != gfx::kNullAcceleratedWidget) {
    display_compositor_.reset(new DisplayCompositor(
        base::ThreadTaskRunnerHandle::Get(), widget, surfaces_state_));
  }
}

void FrameGenerator::RequestCopyOfOutput(
    std::unique_ptr<cc::CopyOutputRequest> output_request) {
  if (display_compositor_)
    display_compositor_->RequestCopyOfOutput(std::move(output_request));
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

  const ViewportMetrics& metrics = delegate_->GetViewportMetrics();
  const gfx::Rect& output_rect = metrics.bounds;
  dirty_rect_.Intersect(output_rect);
  // TODO(fsamuel): We should add a trace for generating a top level frame.
  cc::CompositorFrame frame(GenerateCompositorFrame(output_rect));
  if (frame.metadata.may_contain_video != may_contain_video_) {
    may_contain_video_ = frame.metadata.may_contain_video;
    // TODO(sad): Schedule notifying observers.
    if (may_contain_video_) {
      // TODO(sad): Start a timer to reset the bit if no new frame with video
      // is submitted 'soon'.
    }
  }
  frame_pending_ = true;
  if (display_compositor_) {
    display_compositor_->SubmitCompositorFrame(
        std::move(frame),
        base::Bind(&FrameGenerator::DidDraw, weak_factory_.GetWeakPtr()));
  }
  dirty_rect_ = gfx::Rect();
}

void FrameGenerator::DidDraw() {
  frame_pending_ = false;
  delegate_->OnCompositorFrameDrawn();
  if (!dirty_rect_.IsEmpty())
    WantToDraw();
}

cc::CompositorFrame FrameGenerator::GenerateCompositorFrame(
    const gfx::Rect& output_rect) const {
  const cc::RenderPassId render_pass_id(1, 1);
  std::unique_ptr<cc::RenderPass> render_pass = cc::RenderPass::Create();
  render_pass->SetNew(render_pass_id, output_rect, dirty_rect_,
                      gfx::Transform());

  bool may_contain_video = false;
  DrawWindowTree(render_pass.get(), delegate_->GetRootWindow(), gfx::Vector2d(),
                 1.0f, &may_contain_video);

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
                 cc::FilterOperations() /* background_filters */);
    frame_data->render_pass_list.push_back(std::move(invert_pass));
  }

  cc::CompositorFrame frame;
  frame.delegated_frame_data = std::move(frame_data);
  frame.metadata.may_contain_video = may_contain_video;
  return frame;
}

void FrameGenerator::DrawWindowTree(
    cc::RenderPass* pass,
    ServerWindow* window,
    const gfx::Vector2d& parent_to_root_origin_offset,
    float opacity,
    bool* may_contain_video) const {
  if (!window->visible())
    return;

  ServerWindowSurface* default_surface =
      window->surface_manager() ? window->surface_manager()->GetDefaultSurface()
                                : nullptr;

  const gfx::Rect absolute_bounds =
      window->bounds() + parent_to_root_origin_offset;
  const ServerWindow::Windows& children = window->children();
  const float combined_opacity = opacity * window->opacity();
  for (ServerWindow* child : base::Reversed(children)) {
    DrawWindowTree(pass, child, absolute_bounds.OffsetFromOrigin(),
                   combined_opacity, may_contain_video);
  }

  if (!window->surface_manager() || !window->surface_manager()->ShouldDraw())
    return;

  ServerWindowSurface* underlay_surface =
      window->surface_manager()->GetUnderlaySurface();
  if (!default_surface && !underlay_surface)
    return;

  if (default_surface) {
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
    quad->SetAll(sqs, bounds_at_origin /* rect */,
                 gfx::Rect() /* opaque_rect */,
                 bounds_at_origin /* visible_rect */, true /* needs_blending*/,
                 default_surface->id());
    if (default_surface->may_contain_video())
      *may_contain_video = true;
  }
  if (underlay_surface) {
    const gfx::Rect underlay_absolute_bounds =
        absolute_bounds - window->underlay_offset();
    gfx::Transform quad_to_target_transform;
    quad_to_target_transform.Translate(underlay_absolute_bounds.x(),
                                       underlay_absolute_bounds.y());
    cc::SharedQuadState* sqs = pass->CreateAndAppendSharedQuadState();
    const gfx::Rect bounds_at_origin(
        underlay_surface->last_submitted_frame_size());
    sqs->SetAll(quad_to_target_transform,
                bounds_at_origin.size() /* layer_bounds */,
                bounds_at_origin /* visible_layer_bounds */,
                bounds_at_origin /* clip_rect */, false /* is_clipped */,
                combined_opacity, SkXfermode::kSrcOver_Mode,
                0 /* sorting-context_id */);

    auto* quad = pass->CreateAndAppendDrawQuad<cc::SurfaceDrawQuad>();
    quad->SetAll(sqs, bounds_at_origin /* rect */,
                 gfx::Rect() /* opaque_rect */,
                 bounds_at_origin /* visible_rect */, true /* needs_blending*/,
                 underlay_surface->id());
    DCHECK(!underlay_surface->may_contain_video());
  }
}

}  // namespace ws

}  // namespace ui

// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ui/ws/frame_generator.h"

#include <utility>
#include <vector>

#include "base/containers/adapters.h"
#include "cc/output/compositor_frame.h"
#include "cc/quads/render_pass.h"
#include "cc/quads/render_pass_draw_quad.h"
#include "cc/quads/shared_quad_state.h"
#include "cc/quads/surface_draw_quad.h"
#include "services/ui/ws/frame_generator_delegate.h"
#include "services/ui/ws/server_window.h"
#include "services/ui/ws/server_window_compositor_frame_sink_manager.h"
#include "services/ui/ws/server_window_delegate.h"

namespace ui {

namespace ws {

FrameGenerator::FrameGenerator(FrameGeneratorDelegate* delegate,
                               ServerWindow* root_window)
    : delegate_(delegate),
      root_window_(root_window),
      binding_(this),
      weak_factory_(this) {
  DCHECK(delegate_);
}

FrameGenerator::~FrameGenerator() {
  // Invalidate WeakPtrs now to avoid callbacks back into the
  // FrameGenerator during destruction of |compositor_frame_sink_|.
  weak_factory_.InvalidateWeakPtrs();
  compositor_frame_sink_.reset();
}

void FrameGenerator::OnAcceleratedWidgetAvailable(
    gfx::AcceleratedWidget widget) {
  DCHECK_NE(gfx::kNullAcceleratedWidget, widget);
  auto associated_group =
      root_window_->delegate()->GetDisplayCompositorAssociatedGroup();
  cc::mojom::MojoCompositorFrameSinkAssociatedRequest sink_request =
      mojo::MakeRequest(&compositor_frame_sink_, associated_group);
  cc::mojom::DisplayPrivateAssociatedRequest display_request =
      mojo::MakeRequest(&display_private_, associated_group);
  root_window_->CreateDisplayCompositorFrameSink(
      widget, std::move(sink_request), binding_.CreateInterfacePtrAndBind(),
      std::move(display_request));
  // TODO(fsamuel): This means we're always requesting a new BeginFrame signal
  // even when we don't need it. Once surface ID propagation work is done,
  // this will not be necessary because FrameGenerator will only need a
  // BeginFrame if the window manager changes.
  compositor_frame_sink_->SetNeedsBeginFrame(true);
}

void FrameGenerator::OnSurfaceCreated(const cc::SurfaceId& surface_id,
                                      ServerWindow* window) {
  DCHECK(surface_id.is_valid());

  // Only handle embedded surfaces changing here. The display root surface
  // changing is handled immediately after the CompositorFrame is submitted.
  // TODO(samans): Only tell FrameGenerator about WM surface instead of all
  // all surfaces.
  if (window == delegate_->GetActiveRootWindow())
    window_manager_surface_id_ = surface_id;
}

void FrameGenerator::DidReceiveCompositorFrameAck() {}

void FrameGenerator::OnBeginFrame(const cc::BeginFrameArgs& begin_frame_arags) {
  if (!root_window_->visible())
    return;

  // TODO(fsamuel): We should add a trace for generating a top level frame.
  cc::CompositorFrame frame(GenerateCompositorFrame(root_window_->bounds()));

  if (compositor_frame_sink_) {
    gfx::Size frame_size = last_submitted_frame_size_;
    if (!frame.render_pass_list.empty())
      frame_size = frame.render_pass_list[0]->output_rect.size();

    if (!local_frame_id_.is_valid() ||
        frame_size != last_submitted_frame_size_) {
      local_frame_id_ = id_allocator_.GenerateId();
      display_private_->ResizeDisplay(frame_size);
    }

    compositor_frame_sink_->SubmitCompositorFrame(local_frame_id_,
                                                  std::move(frame));
    last_submitted_frame_size_ = frame_size;
  }
}

void FrameGenerator::ReclaimResources(
    const cc::ReturnedResourceArray& resources) {
  // Nothing to do here because FrameGenerator CompositorFrames don't reference
  // any resources.
}

void FrameGenerator::WillDrawSurface() {
  // TODO(fsamuel, staraz): Implement this.
}

cc::CompositorFrame FrameGenerator::GenerateCompositorFrame(
    const gfx::Rect& output_rect) {
  const int render_pass_id = 1;
  std::unique_ptr<cc::RenderPass> render_pass = cc::RenderPass::Create();
  render_pass->SetNew(render_pass_id, output_rect, output_rect,
                      gfx::Transform());

  DrawWindow(render_pass.get(), delegate_->GetActiveRootWindow());

  cc::CompositorFrame frame;
  frame.render_pass_list.push_back(std::move(render_pass));
  if (delegate_->IsInHighContrastMode()) {
    std::unique_ptr<cc::RenderPass> invert_pass = cc::RenderPass::Create();
    invert_pass->SetNew(2, output_rect, output_rect, gfx::Transform());
    cc::SharedQuadState* shared_state =
        invert_pass->CreateAndAppendSharedQuadState();
    shared_state->SetAll(gfx::Transform(), output_rect.size(), output_rect,
                         output_rect, false, 1.f, SkBlendMode::kSrcOver, 0);
    auto* quad = invert_pass->CreateAndAppendDrawQuad<cc::RenderPassDrawQuad>();
    render_pass->filters.Append(cc::FilterOperation::CreateInvertFilter(1.f));
    quad->SetNew(shared_state, output_rect, output_rect, render_pass_id,
                 0 /* mask_resource_id */, gfx::Vector2dF() /* mask_uv_scale */,
                 gfx::Size() /* mask_texture_size */,
                 gfx::Vector2dF() /* filters_scale */,
                 gfx::PointF() /* filters_origin */);
    frame.render_pass_list.push_back(std::move(invert_pass));
  }
  frame.metadata.device_scale_factor = device_scale_factor_;

  if (window_manager_surface_id_.is_valid())
    frame.metadata.referenced_surfaces.push_back(window_manager_surface_id_);

  return frame;
}

void FrameGenerator::DrawWindow(cc::RenderPass* pass, ServerWindow* window) {
  if (!window || !window->visible())
    return;

  if (!window->compositor_frame_sink_manager())
    return;

  cc::SurfaceId default_surface_id =
      window->compositor_frame_sink_manager()->GetLatestSurfaceId();

  if (!default_surface_id.is_valid())
    return;

  gfx::Transform quad_to_target_transform;
  quad_to_target_transform.Translate(window->bounds().x(),
                                     window->bounds().y());

  cc::SharedQuadState* sqs = pass->CreateAndAppendSharedQuadState();

  const gfx::Rect bounds_at_origin(window->bounds().size());
  // TODO(fsamuel): These clipping and visible rects are incorrect. They need
  // to be populated from CompositorFrame structs.
  sqs->SetAll(
      quad_to_target_transform, bounds_at_origin.size() /* layer_bounds */,
      bounds_at_origin /* visible_layer_bounds */,
      bounds_at_origin /* clip_rect */, false /* is_clipped */,
      1.0f /* opacity */, SkBlendMode::kSrcOver, 0 /* sorting-context_id */);
  auto* quad = pass->CreateAndAppendDrawQuad<cc::SurfaceDrawQuad>();
  quad->SetAll(sqs, bounds_at_origin /* rect */,
               gfx::Rect() /* opaque_rect */,
               bounds_at_origin /* visible_rect */, true /* needs_blending*/,
               default_surface_id);
}

void FrameGenerator::OnWindowDestroying(ServerWindow* window) {
  Remove(window);
}

}  // namespace ws

}  // namespace ui

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
                               ServerWindow* root_window,
                               gfx::AcceleratedWidget widget)
    : delegate_(delegate), root_window_(root_window), binding_(this) {
  DCHECK(delegate_);
  DCHECK_NE(gfx::kNullAcceleratedWidget, widget);
  cc::mojom::MojoCompositorFrameSinkAssociatedRequest sink_request =
      mojo::MakeRequest(&compositor_frame_sink_);
  cc::mojom::DisplayPrivateAssociatedRequest display_request =
      mojo::MakeRequest(&display_private_);
  root_window_->CreateRootCompositorFrameSink(
      widget, std::move(sink_request), binding_.CreateInterfacePtrAndBind(),
      std::move(display_request));
}

FrameGenerator::~FrameGenerator() = default;

void FrameGenerator::SetDeviceScaleFactor(float device_scale_factor) {
  if (device_scale_factor_ == device_scale_factor)
    return;
  device_scale_factor_ = device_scale_factor;
  if (window_manager_surface_info_.is_valid())
    compositor_frame_sink_->SetNeedsBeginFrame(true);
}

void FrameGenerator::OnSurfaceCreated(const cc::SurfaceInfo& surface_info) {
  DCHECK(surface_info.is_valid());

  // Only handle embedded surfaces changing here. The display root surface
  // changing is handled immediately after the CompositorFrame is submitted.
  if (surface_info != window_manager_surface_info_) {
    window_manager_surface_info_ = surface_info;
    compositor_frame_sink_->SetNeedsBeginFrame(true);
  }
}

void FrameGenerator::OnWindowDamaged() {
  if (window_manager_surface_info_.is_valid())
    compositor_frame_sink_->SetNeedsBeginFrame(true);
}

void FrameGenerator::DidReceiveCompositorFrameAck() {}

void FrameGenerator::OnBeginFrame(const cc::BeginFrameArgs& begin_frame_arags) {
  if (!root_window_->visible())
    return;

  // TODO(fsamuel): We should add a trace for generating a top level frame.
  cc::CompositorFrame frame(GenerateCompositorFrame(root_window_->bounds()));

  gfx::Size frame_size = last_submitted_frame_size_;
  if (!frame.render_pass_list.empty())
    frame_size = frame.render_pass_list.back()->output_rect.size();

  if (!local_surface_id_.is_valid() ||
      frame_size != last_submitted_frame_size_) {
    local_surface_id_ = id_allocator_.GenerateId();
    display_private_->ResizeDisplay(frame_size);
  }

  display_private_->SetLocalSurfaceId(local_surface_id_, device_scale_factor_);
  compositor_frame_sink_->SubmitCompositorFrame(local_surface_id_,
                                                std::move(frame));
  compositor_frame_sink_->SetNeedsBeginFrame(false);
  last_submitted_frame_size_ = frame_size;
}

void FrameGenerator::ReclaimResources(
    const cc::ReturnedResourceArray& resources) {
  // Nothing to do here because FrameGenerator CompositorFrames don't reference
  // any resources.
  DCHECK(resources.empty());
}

void FrameGenerator::WillDrawSurface(const cc::LocalSurfaceId& local_surface_id,
                                     const gfx::Rect& damage_rect) {
  // TODO(fsamuel, staraz): Implement this.
}

cc::CompositorFrame FrameGenerator::GenerateCompositorFrame(
    const gfx::Rect& output_rect) {
  const int render_pass_id = 1;
  std::unique_ptr<cc::RenderPass> render_pass = cc::RenderPass::Create();
  render_pass->SetNew(render_pass_id, output_rect, output_rect,
                      gfx::Transform());

  DrawWindow(render_pass.get());

  cc::CompositorFrame frame;
  frame.render_pass_list.push_back(std::move(render_pass));
  if (delegate_->IsInHighContrastMode()) {
    std::unique_ptr<cc::RenderPass> invert_pass = cc::RenderPass::Create();
    invert_pass->SetNew(2, output_rect, output_rect, gfx::Transform());
    cc::SharedQuadState* shared_state =
        invert_pass->CreateAndAppendSharedQuadState();
    gfx::Size scaled_bounds = gfx::ScaleToCeiledSize(
        output_rect.size(), window_manager_surface_info_.device_scale_factor(),
        window_manager_surface_info_.device_scale_factor());
    shared_state->SetAll(gfx::Transform(), scaled_bounds, output_rect,
                         output_rect, false, 1.f, SkBlendMode::kSrcOver, 0);
    auto* quad = invert_pass->CreateAndAppendDrawQuad<cc::RenderPassDrawQuad>();
    render_pass->filters.Append(cc::FilterOperation::CreateInvertFilter(1.f));
    quad->SetNew(shared_state, output_rect, output_rect, render_pass_id,
                 0 /* mask_resource_id */, gfx::RectF() /* mask_uv_rect */,
                 gfx::Size() /* mask_texture_size */,
                 gfx::Vector2dF() /* filters_scale */,
                 gfx::PointF() /* filters_origin */,
                 gfx::RectF() /* tex_coord_rect */);
    frame.render_pass_list.push_back(std::move(invert_pass));
  }
  frame.metadata.device_scale_factor = device_scale_factor_;

  if (window_manager_surface_info_.is_valid()) {
    frame.metadata.referenced_surfaces.push_back(
        window_manager_surface_info_.id());
  }

  return frame;
}

void FrameGenerator::DrawWindow(cc::RenderPass* pass) {
  DCHECK(window_manager_surface_info_.is_valid());

  const gfx::Rect bounds_at_origin(
      window_manager_surface_info_.size_in_pixels());

  gfx::Transform quad_to_target_transform;
  quad_to_target_transform.Translate(bounds_at_origin.x(),
                                     bounds_at_origin.y());

  cc::SharedQuadState* sqs = pass->CreateAndAppendSharedQuadState();

  gfx::Size scaled_bounds = gfx::ScaleToCeiledSize(
      bounds_at_origin.size(),
      window_manager_surface_info_.device_scale_factor(),
      window_manager_surface_info_.device_scale_factor());

  // TODO(fsamuel): These clipping and visible rects are incorrect. They need
  // to be populated from CompositorFrame structs.
  sqs->SetAll(quad_to_target_transform, scaled_bounds /* layer_bounds */,
              bounds_at_origin /* visible_layer_bounds */,
              bounds_at_origin /* clip_rect */, false /* is_clipped */,
              1.0f /* opacity */, SkBlendMode::kSrcOver,
              0 /* sorting-context_id */);
  auto* quad = pass->CreateAndAppendDrawQuad<cc::SurfaceDrawQuad>();
  quad->SetAll(sqs, bounds_at_origin /* rect */, gfx::Rect() /* opaque_rect */,
               bounds_at_origin /* visible_rect */, true /* needs_blending*/,
               window_manager_surface_info_.id(),
               cc::SurfaceDrawQuadType::PRIMARY, nullptr);
}

}  // namespace ws

}  // namespace ui

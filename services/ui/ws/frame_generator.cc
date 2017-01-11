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
  // Remove reference from top level root to the display root surface, if one
  // exists. This will make everything referenced by the display surface
  // unreachable so it can be garbage collected.
  if (surface_tracker_.HasValidSurfaceId()) {
    compositor_frame_sink_->RemoveSurfaceReferences(
        std::vector<cc::SurfaceReference>{
            cc::SurfaceReference(root_window_->delegate()->GetRootSurfaceId(),
                                 surface_tracker_.current_surface_id())});
  }

  // Invalidate WeakPtrs now to avoid callbacks back into the
  // FrameGenerator during destruction of |compositor_frame_sink_|.
  weak_factory_.InvalidateWeakPtrs();
  compositor_frame_sink_.reset();
}

void FrameGenerator::OnAcceleratedWidgetAvailable(
    gfx::AcceleratedWidget widget) {
  DCHECK_NE(gfx::kNullAcceleratedWidget, widget);
  cc::mojom::MojoCompositorFrameSinkRequest request(&compositor_frame_sink_);
  cc::mojom::DisplayPrivateRequest display_private_request(&display_private_);
  root_window_->CreateDisplayCompositorFrameSink(
      widget, std::move(request), binding_.CreateInterfacePtrAndBind(),
      std::move(display_private_request));
  // TODO(fsamuel): This means we're always requesting a new BeginFrame signal
  // even when we don't need it. Once surface ID propagation work is done,
  // this will not be necessary because FrameGenerator will only need a
  // BeginFrame if the window manager changes.
  compositor_frame_sink_->SetNeedsBeginFrame(true);
}

void FrameGenerator::OnSurfaceCreated(const cc::SurfaceId& surface_id,
                                      ServerWindow* window) {
  DCHECK(surface_id.is_valid());

  // TODO(samans): Clients are actually embedded in the WM and only the WM is
  // embedded here. This needs to be fixed.

  // Only handle embedded surfaces changing here. The display root surface
  // changing is handled immediately after the CompositorFrame is submitted.
  if (window != root_window_) {
    // Add observer for window the first time it's seen.
    if (surface_tracker_.EmbedSurface(surface_id))
      Add(window);
  }
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

    bool display_surface_changed = false;
    if (!local_frame_id_.is_valid() ||
        frame_size != last_submitted_frame_size_) {
      local_frame_id_ = id_allocator_.GenerateId();
      display_surface_changed = true;
    } else {
      // If the display surface is changing then we shouldn't add references
      // from the old display surface. We want to add references from the new
      // display surface, this happens after we submit the first CompositorFrame
      // so the new display surface exists.
      if (surface_tracker_.HasReferencesToAdd()) {
        compositor_frame_sink_->AddSurfaceReferences(
            surface_tracker_.GetReferencesToAdd());
      }
    }

    compositor_frame_sink_->SubmitCompositorFrame(local_frame_id_,
                                                  std::move(frame));
    last_submitted_frame_size_ = frame_size;

    if (display_surface_changed)
      UpdateDisplaySurfaceId();

    // Remove references to surfaces that are no longer embedded. This has to
    // happen after the frame is submitted otherwise we could end up deleting
    // a surface that is still embedded in the last submitted frame.
    if (surface_tracker_.HasReferencesToRemove()) {
      compositor_frame_sink_->RemoveSurfaceReferences(
          surface_tracker_.GetReferencesToRemove());
    }
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

void FrameGenerator::UpdateDisplaySurfaceId() {
  // FrameGenerator owns the display root surface and is a bit of a special
  // case. There is no surface that embeds the display surface, so nothing
  // would reference it. Instead, a reference from the top level root is added
  // to mark the display surface as visible. As a result, FrameGenerator is
  // responsible for maintaining a reference from the top level root to the
  // display surface, in addition to references from the display surface to
  // embedded surfaces.
  const cc::SurfaceId old_surface_id = surface_tracker_.current_surface_id();
  const cc::SurfaceId new_surface_id(
      cc::FrameSinkId(WindowIdToTransportId(root_window_->id()), 0),
      local_frame_id_);

  DCHECK_NE(old_surface_id, new_surface_id);

  // Set new SurfaceId for the display surface. This will add references from
  // the new display surface to all embedded surfaces.
  surface_tracker_.SetCurrentSurfaceId(new_surface_id);
  std::vector<cc::SurfaceReference> references_to_add =
      surface_tracker_.GetReferencesToAdd();

  // Adds a reference from the top level root to the new display surface.
  references_to_add.push_back(cc::SurfaceReference(
      root_window_->delegate()->GetRootSurfaceId(), new_surface_id));

  compositor_frame_sink_->AddSurfaceReferences(references_to_add);

  // Remove the reference from the top level root to the old display surface
  // after we have added references from the new display surface. Not applicable
  // for the first display surface.
  if (old_surface_id.is_valid()) {
    compositor_frame_sink_->RemoveSurfaceReferences(
        std::vector<cc::SurfaceReference>{cc::SurfaceReference(
            root_window_->delegate()->GetRootSurfaceId(), old_surface_id)});
  }
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
  ServerWindowCompositorFrameSinkManager* compositor_frame_sink_manager =
      window->compositor_frame_sink_manager();
  // If FrameGenerator was observing |window|, then that means it had a
  // CompositorFrame at some point in time and should have a
  // ServerWindowCompositorFrameSinkManager.
  DCHECK(compositor_frame_sink_manager);

  cc::SurfaceId surface_id =
      window->compositor_frame_sink_manager()->GetLatestSurfaceId();
  if (surface_id.is_valid())
    surface_tracker_.UnembedSurface(surface_id.frame_sink_id());
}

}  // namespace ws

}  // namespace ui

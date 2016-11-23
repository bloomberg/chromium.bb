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
      top_level_root_surface_id_(
          cc::FrameSinkId(0, 0),
          cc::LocalFrameId(0, base::UnguessableToken::Create())),
      binding_(this),
      weak_factory_(this) {
  DCHECK(delegate_);
}

FrameGenerator::~FrameGenerator() {
  RemoveDeadSurfaceReferences();
  RemoveAllSurfaceReferences();
  // Invalidate WeakPtrs now to avoid callbacks back into the
  // FrameGenerator during destruction of |compositor_frame_sink_|.
  weak_factory_.InvalidateWeakPtrs();
  compositor_frame_sink_.reset();
}

void FrameGenerator::OnAcceleratedWidgetAvailable(
    gfx::AcceleratedWidget widget) {
  DCHECK_NE(gfx::kNullAcceleratedWidget, widget);
  cc::mojom::MojoCompositorFrameSinkRequest request =
      mojo::GetProxy(&compositor_frame_sink_);
  root_window_->CreateCompositorFrameSink(
      mojom::CompositorFrameSinkType::DEFAULT, widget, std::move(request),
      binding_.CreateInterfacePtrAndBind());
  // TODO(fsamuel): This means we're always requesting a new BeginFrame signal
  // even when we don't need it. Once surface ID propagation work is done,
  // this will not be necessary because FrameGenerator will only need a
  // BeginFrame if the window manager changes.
  compositor_frame_sink_->SetNeedsBeginFrame(true);
}

void FrameGenerator::OnSurfaceCreated(const cc::SurfaceId& surface_id,
                                      ServerWindow* window) {
  DCHECK(surface_id.is_valid());

  // TODO(kylechar): Adding surface references should be synchronized with
  // SubmitCompositorFrame().

  auto iter = active_references_.find(surface_id.frame_sink_id());
  if (iter == active_references_.end()) {
    AddFirstReference(surface_id, window);
    return;
  }

  SurfaceReference& ref = iter->second;
  DCHECK_EQ(surface_id.frame_sink_id(), ref.child_id.frame_sink_id());

  // This shouldn't be called multiple times for the same SurfaceId.
  DCHECK_NE(surface_id.local_frame_id(), ref.child_id.local_frame_id());

  // Add a reference from parent to new surface first.
  GetDisplayCompositor()->AddSurfaceReference(ref.parent_id, surface_id);

  // If the display root surface has changed, update all references to embedded
  // surfaces. For example, this would happen when the display resolution or
  // zoom level changes.
  if (!window->parent())
    AddNewParentReferences(ref.child_id, surface_id);

  // Move the existing reference to list of references to remove after we submit
  // the next CompositorFrame. Update local reference cache to be the new
  // reference. If this is the display root surface then removing this reference
  // will recursively remove any references it held.
  dead_references_.push_back(ref);
  ref.child_id = surface_id;
}

void FrameGenerator::DidReceiveCompositorFrameAck() {}

void FrameGenerator::OnBeginFrame(const cc::BeginFrameArgs& begin_frame_arags) {
  if (!root_window_->visible())
    return;

  // TODO(fsamuel): We should add a trace for generating a top level frame.
  cc::CompositorFrame frame(GenerateCompositorFrame(root_window_->bounds()));
  if (compositor_frame_sink_) {
    compositor_frame_sink_->SubmitCompositorFrame(std::move(frame));

    // Remove dead references after we submit a frame. This has to happen after
    // the frame is submitted otherwise we could end up deleting a surface that
    // is still embedded in the last frame.
    // TODO(kylechar): This should be synchronized with SubmitCompositorFrame().
    RemoveDeadSurfaceReferences();
  }
}

void FrameGenerator::ReclaimResources(
    const cc::ReturnedResourceArray& resources) {
  // Nothing to do here because FrameGenerator CompositorFrames don't reference
  // any resources.
}

cc::CompositorFrame FrameGenerator::GenerateCompositorFrame(
    const gfx::Rect& output_rect) {
  const cc::RenderPassId render_pass_id(1, 1);
  std::unique_ptr<cc::RenderPass> render_pass = cc::RenderPass::Create();
  render_pass->SetNew(render_pass_id, output_rect, output_rect,
                      gfx::Transform());

  DrawWindowTree(render_pass.get(), root_window_, gfx::Vector2d(), 1.0f);

  cc::CompositorFrame frame;
  frame.render_pass_list.push_back(std::move(render_pass));
  if (delegate_->IsInHighContrastMode()) {
    std::unique_ptr<cc::RenderPass> invert_pass = cc::RenderPass::Create();
    invert_pass->SetNew(cc::RenderPassId(2, 0), output_rect, output_rect,
                        gfx::Transform());
    cc::SharedQuadState* shared_state =
        invert_pass->CreateAndAppendSharedQuadState();
    shared_state->SetAll(gfx::Transform(), output_rect.size(), output_rect,
                         output_rect, false, 1.f, SkBlendMode::kSrcOver, 0);
    auto* quad = invert_pass->CreateAndAppendDrawQuad<cc::RenderPassDrawQuad>();
    cc::FilterOperations filters;
    filters.Append(cc::FilterOperation::CreateInvertFilter(1.f));
    quad->SetNew(shared_state, output_rect, output_rect, render_pass_id,
                 0 /* mask_resource_id */, gfx::Vector2dF() /* mask_uv_scale */,
                 gfx::Size() /* mask_texture_size */, filters,
                 gfx::Vector2dF() /* filters_scale */,
                 gfx::PointF() /* filters_origin */,
                 cc::FilterOperations() /* background_filters */);
    frame.render_pass_list.push_back(std::move(invert_pass));
  }

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

  if (!underlay_surface_id.is_valid() && !default_surface_id.is_valid())
    return;

  if (default_surface_id.is_valid()) {
    gfx::Transform quad_to_target_transform;
    quad_to_target_transform.Translate(absolute_bounds.x(),
                                       absolute_bounds.y());

    cc::SharedQuadState* sqs = pass->CreateAndAppendSharedQuadState();

    const gfx::Rect bounds_at_origin(window->bounds().size());
    // TODO(fsamuel): These clipping and visible rects are incorrect. They need
    // to be populated from CompositorFrame structs.
    sqs->SetAll(
        quad_to_target_transform, bounds_at_origin.size() /* layer_bounds */,
        bounds_at_origin /* visible_layer_bounds */,
        bounds_at_origin /* clip_rect */, false /* is_clipped */,
        combined_opacity, SkBlendMode::kSrcOver, 0 /* sorting-context_id */);
    auto* quad = pass->CreateAndAppendDrawQuad<cc::SurfaceDrawQuad>();
    quad->SetAll(sqs, bounds_at_origin /* rect */,
                 gfx::Rect() /* opaque_rect */,
                 bounds_at_origin /* visible_rect */, true /* needs_blending*/,
                 default_surface_id);
  }
  if (underlay_surface_id.is_valid()) {
    const gfx::Rect underlay_absolute_bounds =
        absolute_bounds - window->underlay_offset();
    gfx::Transform quad_to_target_transform;
    quad_to_target_transform.Translate(underlay_absolute_bounds.x(),
                                       underlay_absolute_bounds.y());
    cc::SharedQuadState* sqs = pass->CreateAndAppendSharedQuadState();
    const gfx::Rect bounds_at_origin(
        window->compositor_frame_sink_manager()->GetLatestFrameSize(
            mojom::CompositorFrameSinkType::UNDERLAY));
    sqs->SetAll(
        quad_to_target_transform, bounds_at_origin.size() /* layer_bounds */,
        bounds_at_origin /* visible_layer_bounds */,
        bounds_at_origin /* clip_rect */, false /* is_clipped */,
        combined_opacity, SkBlendMode::kSrcOver, 0 /* sorting-context_id */);

    auto* quad = pass->CreateAndAppendDrawQuad<cc::SurfaceDrawQuad>();
    quad->SetAll(sqs, bounds_at_origin /* rect */,
                 gfx::Rect() /* opaque_rect */,
                 bounds_at_origin /* visible_rect */, true /* needs_blending*/,
                 underlay_surface_id);
  }
}

cc::SurfaceId FrameGenerator::FindParentSurfaceId(ServerWindow* window) {
  if (window == root_window_)
    return top_level_root_surface_id_;

  // The root window holds the parent SurfaceId. This SurfaceId will have an
  // invalid LocalFrameId before FrameGenerator has submitted a CompositorFrame.
  // After the first frame is submitted it will always be a valid SurfaceId.
  return root_window_->compositor_frame_sink_manager()->GetLatestSurfaceId(
      mojom::CompositorFrameSinkType::DEFAULT);
}

void FrameGenerator::AddSurfaceReference(const cc::SurfaceId& parent_id,
                                         const cc::SurfaceId& child_id) {
  if (parent_id == top_level_root_surface_id_)
    GetDisplayCompositor()->AddRootSurfaceReference(child_id);
  else
    GetDisplayCompositor()->AddSurfaceReference(parent_id, child_id);

  // Add new reference from parent to surface, plus add reference to local
  // cache.
  active_references_[child_id.frame_sink_id()] =
      SurfaceReference({parent_id, child_id});
}

void FrameGenerator::AddFirstReference(const cc::SurfaceId& surface_id,
                                       ServerWindow* window) {
  cc::SurfaceId parent_id = FindParentSurfaceId(window);

  if (parent_id == top_level_root_surface_id_ || parent_id.is_valid()) {
    AddSurfaceReference(parent_id, surface_id);

    // For the first display root surface, add references to any child surfaces
    // that were created before it.
    if (parent_id == top_level_root_surface_id_) {
      for (auto& child_surface_id : waiting_for_references_)
        AddSurfaceReference(surface_id, child_surface_id);
      waiting_for_references_.clear();
    }
  } else {
    // This isn't the display root surface and display root surface hasn't
    // submitted a CF yet. We can't add a reference to an unknown SurfaceId.
    waiting_for_references_.push_back(surface_id);
  }

  // Observe |window| so that we can remove references when it's destroyed.
  Add(window);
}

void FrameGenerator::AddNewParentReferences(
    const cc::SurfaceId& old_surface_id,
    const cc::SurfaceId& new_surface_id) {
  DCHECK(old_surface_id.frame_sink_id() == new_surface_id.frame_sink_id());

  cc::mojom::DisplayCompositor* display_compositor = GetDisplayCompositor();
  for (auto& map_entry : active_references_) {
    SurfaceReference& ref = map_entry.second;
    if (ref.parent_id == old_surface_id) {
      display_compositor->AddSurfaceReference(new_surface_id, ref.child_id);
      ref.parent_id = new_surface_id;
    }
  }
}

void FrameGenerator::RemoveDeadSurfaceReferences() {
  if (dead_references_.empty())
    return;

  cc::mojom::DisplayCompositor* display_compositor = GetDisplayCompositor();
  for (auto& ref : dead_references_) {
    if (ref.parent_id == top_level_root_surface_id_)
      display_compositor->RemoveRootSurfaceReference(ref.child_id);
    else
      display_compositor->RemoveSurfaceReference(ref.parent_id, ref.child_id);
  }
  dead_references_.clear();
}

void FrameGenerator::RemoveFrameSinkReference(
    const cc::FrameSinkId& frame_sink_id) {
  auto it = active_references_.find(frame_sink_id);
  if (it == active_references_.end())
    return;
  dead_references_.push_back(it->second);
  active_references_.erase(it);
}

void FrameGenerator::RemoveAllSurfaceReferences() {
  // TODO(kylechar): Remove multiple surfaces with one IPC call.
  cc::mojom::DisplayCompositor* display_compositor = GetDisplayCompositor();
  for (auto& map_entry : active_references_) {
    const SurfaceReference& ref = map_entry.second;
    display_compositor->RemoveSurfaceReference(ref.parent_id, ref.child_id);
  }
  active_references_.clear();
}

cc::mojom::DisplayCompositor* FrameGenerator::GetDisplayCompositor() {
  return root_window_->delegate()->GetDisplayCompositor();
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
  if (default_surface_id.is_valid())
    RemoveFrameSinkReference(default_surface_id.frame_sink_id());

  cc::SurfaceId underlay_surface_id =
      window->compositor_frame_sink_manager()->GetLatestSurfaceId(
          mojom::CompositorFrameSinkType::UNDERLAY);
  if (underlay_surface_id.is_valid())
    RemoveFrameSinkReference(underlay_surface_id.frame_sink_id());
}

}  // namespace ws

}  // namespace ui

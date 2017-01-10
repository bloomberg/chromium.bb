// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ui/ws/frame_generator.h"

#include <utility>

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
  RemoveAllSurfaceReferences();
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

  auto iter = active_references_.find(surface_id.frame_sink_id());
  if (iter == active_references_.end()) {
    AddFirstReference(surface_id, window);
    return;
  }

  cc::SurfaceReference& ref = iter->second;

  // This shouldn't be called multiple times for the same SurfaceId.
  DCHECK_EQ(surface_id.frame_sink_id(), ref.child_id().frame_sink_id());
  DCHECK_NE(surface_id.local_frame_id(), ref.child_id().local_frame_id());

  // The current reference will be removed after the next CompositorFrame is
  // submitted or FrameGenerator is destroyed.
  references_to_remove_.push_back(ref);
  cc::SurfaceId old_surface_id = ref.child_id();

  // New surface reference is recorded and will be added at end of this method.
  ref = cc::SurfaceReference(ref.parent_id(), surface_id);
  references_to_add_.push_back(ref);

  // If the display root surface has changed, add references from the new
  // SurfaceId to all embedded surfaces. For example, this would happen when the
  // display resolution or device scale factor changes.
  if (window == root_window_)
    AddNewParentReferences(old_surface_id, surface_id);

  PerformAddSurfaceReferences();
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
    if (!local_frame_id_.is_valid() || frame_size != last_submitted_frame_size_)
      local_frame_id_ = id_allocator_.GenerateId();
    compositor_frame_sink_->SubmitCompositorFrame(local_frame_id_,
                                                  std::move(frame));
    last_submitted_frame_size_ = frame_size;

    // Remove dead references after we submit a frame. This has to happen after
    // the frame is submitted otherwise we could end up deleting a surface that
    // is still embedded in the last frame.
    PerformRemoveSurfaceReferences();
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

cc::SurfaceId FrameGenerator::FindParentSurfaceId(ServerWindow* window) {
  if (window == root_window_)
    return root_window_->delegate()->GetRootSurfaceId();

  // The root window holds the parent SurfaceId. This SurfaceId will have an
  // invalid LocalFrameId before FrameGenerator has submitted a CompositorFrame.
  // After the first frame is submitted it will always be a valid SurfaceId.
  return root_window_->compositor_frame_sink_manager()->GetLatestSurfaceId();
}

void FrameGenerator::AddSurfaceReference(const cc::SurfaceId& parent_id,
                                         const cc::SurfaceId& child_id) {
  DCHECK_NE(parent_id, child_id);

  // Add new reference from parent to surface and record reference.
  cc::SurfaceReference ref(parent_id, child_id);
  active_references_[child_id.frame_sink_id()] = ref;
  references_to_add_.push_back(ref);
}

void FrameGenerator::AddFirstReference(const cc::SurfaceId& surface_id,
                                       ServerWindow* window) {
  cc::SurfaceId parent_id = FindParentSurfaceId(window);

  if (parent_id.local_frame_id().is_valid()) {
    AddSurfaceReference(parent_id, surface_id);

    // For the first display root surface, add references to any child surfaces
    // that were created before it, since no reference has been added yet.
    if (window == root_window_) {
      for (auto& child_surface_id : waiting_for_references_)
        AddSurfaceReference(surface_id, child_surface_id);
      waiting_for_references_.clear();
    }

    PerformAddSurfaceReferences();
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
  DCHECK_EQ(old_surface_id.frame_sink_id(), new_surface_id.frame_sink_id());

  for (auto& map_entry : active_references_) {
    cc::SurfaceReference& ref = map_entry.second;
    if (ref.parent_id() == old_surface_id) {
      ref = cc::SurfaceReference(new_surface_id, ref.child_id());
      references_to_add_.push_back(ref);
    }
  }
}

void FrameGenerator::PerformAddSurfaceReferences() {
  if (references_to_add_.empty())
    return;

  compositor_frame_sink_->AddSurfaceReferences(references_to_add_);
  references_to_add_.clear();
}

void FrameGenerator::PerformRemoveSurfaceReferences() {
  if (references_to_remove_.empty())
    return;

  compositor_frame_sink_->RemoveSurfaceReferences(references_to_remove_);
  references_to_remove_.clear();
}

void FrameGenerator::RemoveFrameSinkReference(
    const cc::FrameSinkId& frame_sink_id) {
  auto it = active_references_.find(frame_sink_id);
  if (it == active_references_.end())
    return;
  references_to_remove_.push_back(it->second);
  active_references_.erase(it);
}

void FrameGenerator::RemoveAllSurfaceReferences() {
  for (auto& map_entry : active_references_)
    references_to_remove_.push_back(map_entry.second);
  active_references_.clear();
  PerformRemoveSurfaceReferences();
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
    RemoveFrameSinkReference(surface_id.frame_sink_id());
}

}  // namespace ws

}  // namespace ui

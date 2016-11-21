// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ui/surfaces/display_compositor.h"

#include "cc/surfaces/surface.h"

namespace ui {

DisplayCompositor::DisplayCompositor(
    cc::mojom::DisplayCompositorClientPtr client)
    : client_(std::move(client)) {
  manager_.AddObserver(this);
}

void DisplayCompositor::AddRootSurfaceReference(const cc::SurfaceId& child_id) {
  AddSurfaceReference(manager_.GetRootSurfaceId(), child_id);
}

void DisplayCompositor::AddSurfaceReference(const cc::SurfaceId& parent_id,
                                            const cc::SurfaceId& child_id) {
  auto vector_iter = temp_references_.find(child_id.frame_sink_id());

  // If there are no temporary references for the FrameSinkId then we can just
  // add reference and return.
  if (vector_iter == temp_references_.end()) {
    manager_.AddSurfaceReference(parent_id, child_id);
    return;
  }

  // Get the vector<LocalFrameId> for the appropriate FrameSinkId and look for
  // |child_id.local_frame_id| in that vector. If found, there is a temporary
  // reference to |child_id|.
  std::vector<cc::LocalFrameId>& refs = vector_iter->second;
  auto temp_ref_iter =
      std::find(refs.begin(), refs.end(), child_id.local_frame_id());

  if (temp_ref_iter == refs.end()) {
    manager_.AddSurfaceReference(parent_id, child_id);
    return;
  }

  // All surfaces get a temporary reference to the top level root. If the parent
  // wants to add a reference to the top level root then we do nothing.
  // Otherwise remove the temporary reference and add the reference.
  if (parent_id != manager_.GetRootSurfaceId()) {
    manager_.AddSurfaceReference(parent_id, child_id);
    manager_.RemoveSurfaceReference(manager_.GetRootSurfaceId(), child_id);
  }

  // Remove temporary references for surfaces with the same FrameSinkId that
  // were created before |child_id|. The earlier surfaces were never embedded in
  // the parent and the parent is embedding a later surface, so we know the
  // parent doesn't need them anymore.
  for (auto iter = refs.begin(); iter != temp_ref_iter; ++iter) {
    cc::SurfaceId id = cc::SurfaceId(child_id.frame_sink_id(), *iter);
    manager_.RemoveSurfaceReference(manager_.GetRootSurfaceId(), id);
  }

  // Remove markers for temporary references up to |child_id|, as the temporary
  // references they correspond to were removed above. If |ref_iter| is the last
  // element in |refs| then we are removing all temporary references for the
  // FrameSinkId and can remove the map entry entirely.
  if (++temp_ref_iter == refs.end())
    temp_references_.erase(child_id.frame_sink_id());
  else
    refs.erase(refs.begin(), ++temp_ref_iter);
}

void DisplayCompositor::RemoveRootSurfaceReference(
    const cc::SurfaceId& child_id) {
  RemoveSurfaceReference(manager_.GetRootSurfaceId(), child_id);
}

void DisplayCompositor::RemoveSurfaceReference(const cc::SurfaceId& parent_id,
                                               const cc::SurfaceId& child_id) {
  // TODO(kylechar): Each remove reference can trigger GC, it would be better if
  // we GC only once if removing multiple references.
  manager_.RemoveSurfaceReference(parent_id, child_id);
}

DisplayCompositor::~DisplayCompositor() {
  // Remove all temporary references on shutdown.
  for (auto& map_entry : temp_references_) {
    const cc::FrameSinkId& frame_sink_id = map_entry.first;
    for (auto& local_frame_id : map_entry.second) {
      manager_.RemoveSurfaceReference(
          manager_.GetRootSurfaceId(),
          cc::SurfaceId(frame_sink_id, local_frame_id));
    }
  }
  manager_.RemoveObserver(this);
}

void DisplayCompositor::OnSurfaceCreated(const cc::SurfaceId& surface_id,
                                         const gfx::Size& frame_size,
                                         float device_scale_factor) {
  // We can get into a situation where multiple CompositorFrames arrive for a
  // CompositorFrameSink before the DisplayCompositorClient can add any
  // references for the frame. When the second frame with a new size arrives,
  // the first will be destroyed and then if there are no references it will be
  // deleted during surface GC. A temporary reference, removed when a real
  // reference is received, is added to prevent this from happening.
  manager_.AddSurfaceReference(manager_.GetRootSurfaceId(), surface_id);
  temp_references_[surface_id.frame_sink_id()].push_back(
      surface_id.local_frame_id());

  if (client_)
    client_->OnSurfaceCreated(surface_id, frame_size, device_scale_factor);
}

void DisplayCompositor::OnSurfaceDamaged(const cc::SurfaceId& surface_id,
                                         bool* changed) {}

}  // namespace ui

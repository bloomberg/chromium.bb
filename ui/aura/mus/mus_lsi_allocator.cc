// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/mus/mus_lsi_allocator.h"

#include <utility>

#include "components/viz/common/surfaces/scoped_surface_id_allocator.h"
#include "components/viz/common/surfaces/surface_id.h"
#include "ui/aura/mus/client_surface_embedder.h"
#include "ui/aura/mus/window_port_mus.h"
#include "ui/aura/mus/window_tree_client.h"

namespace aura {

ParentAllocator::ParentAllocator(MusLsiAllocatorType type,
                                 WindowPortMus* window,
                                 WindowTreeClient* window_tree_client)
    : MusLsiAllocator(type),
      window_(window),
      window_tree_client_(window_tree_client) {
  DCHECK(window_);
  DCHECK(window_tree_client_);
  if (type == MusLsiAllocatorType::kEmbed) {
    client_surface_embedder_ = std::make_unique<ClientSurfaceEmbedder>(
        GetWindow(), /* inject_gutter */ false, gfx::Insets());
  }
}

ParentAllocator::~ParentAllocator() = default;

void ParentAllocator::AllocateLocalSurfaceId() {
  last_surface_size_in_pixels_ = window_->GetSizeInPixels();
  parent_local_surface_id_allocator_.GenerateId();
  Update(/* send_bounds_change */ true);
}

viz::ScopedSurfaceIdAllocator ParentAllocator::GetSurfaceIdAllocator(
    base::OnceClosure allocation_task) {
  return viz::ScopedSurfaceIdAllocator(&parent_local_surface_id_allocator_,
                                       std::move(allocation_task));
}

void ParentAllocator::InvalidateLocalSurfaceId() {
  parent_local_surface_id_allocator_.Invalidate();
}

void ParentAllocator::UpdateLocalSurfaceIdFromEmbeddedClient(
    const viz::LocalSurfaceIdAllocation&
        embedded_client_local_surface_id_allocation) {
  parent_local_surface_id_allocator_.UpdateFromChild(
      embedded_client_local_surface_id_allocation);
  // Ensure there is a valid value.
  if (!GetLocalSurfaceIdAllocation().IsValid())
    parent_local_surface_id_allocator_.GenerateId();
  Update(/* send_bounds_change */ true);
}

void ParentAllocator::OnDeviceScaleFactorChanged() {
  parent_local_surface_id_allocator_.GenerateId();
  Update(/* send_bounds_change */ true);
}

void ParentAllocator::OnDidChangeBounds(const gfx::Size& size_in_pixels,
                                        bool from_server) {
  if (last_surface_size_in_pixels_ == size_in_pixels &&
      parent_local_surface_id_allocator_.HasValidLocalSurfaceIdAllocation()) {
    return;
  }

  last_surface_size_in_pixels_ = size_in_pixels;
  parent_local_surface_id_allocator_.GenerateId();
  // If |from_server| is true, then WindowPortMus sends a bound change.
  Update(/* send_bounds_change */ from_server);
}

const viz::LocalSurfaceIdAllocation&
ParentAllocator::GetLocalSurfaceIdAllocation() {
  return parent_local_surface_id_allocator_
      .GetCurrentLocalSurfaceIdAllocation();
}

aura::Window* ParentAllocator::GetWindow() {
  return static_cast<WindowMus*>(window_)->GetWindow();
}

void ParentAllocator::Update(bool send_bounds_change) {
  // If not in a bounds change, then need to update server of new
  // LocalSurfaceId.
  if (send_bounds_change) {
    const gfx::Rect& bounds = GetWindow()->bounds();
    window_tree_client_->OnWindowMusBoundsChanged(window_, bounds, bounds);
  }
  if (GetWindow()->IsEmbeddingClient() && client_surface_embedder_) {
    viz::SurfaceId surface_id(GetWindow()->GetFrameSinkId(),
                              GetLocalSurfaceIdAllocation().local_surface_id());
    client_surface_embedder_->SetSurfaceId(surface_id);
    client_surface_embedder_->UpdateSizeAndGutters();
  }
}

void ParentAllocator::OnFrameSinkIdChanged() {
  Update(/* send_bounds_change */ false);
}

// static
std::unique_ptr<MusLsiAllocator> MusLsiAllocator::CreateAllocator(
    MusLsiAllocatorType type,
    WindowPortMus* window,
    WindowTreeClient* window_tree_client) {
  return std::make_unique<ParentAllocator>(type, window, window_tree_client);
}

}  // namespace aura

// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/mus/mus_lsi_allocator.h"

#include <utility>

#include "base/no_destructor.h"
#include "components/viz/common/surfaces/child_local_surface_id_allocator.h"
#include "components/viz/common/surfaces/local_surface_id_allocation.h"
#include "components/viz/common/surfaces/scoped_surface_id_allocator.h"
#include "components/viz/common/surfaces/surface_id.h"
#include "ui/aura/mus/client_surface_embedder.h"
#include "ui/aura/mus/window_port_mus.h"
#include "ui/aura/mus/window_tree_client.h"
#include "ui/aura/mus/window_tree_host_mus.h"
#include "ui/aura/window_tree_host.h"

namespace aura {

ParentAllocator* MusLsiAllocator::AsParentAllocator() {
  return nullptr;
}

TopLevelAllocator* MusLsiAllocator::AsTopLevelAllocator() {
  return nullptr;
}

MusLsiAllocator::MusLsiAllocator(WindowPortMus* window_port_mus,
                                 WindowTreeClient* window_tree_client)
    : window_port_mus_(window_port_mus),
      window_tree_client_(window_tree_client) {
  DCHECK(window_port_mus_);
  DCHECK(window_tree_client_);
}

aura::Window* MusLsiAllocator::GetWindow() {
  return static_cast<WindowMus*>(window_port_mus_)->GetWindow();
}

// ParentAllocator -------------------------------------------------------------

ParentAllocator::ParentAllocator(WindowPortMus* window_port_mus,
                                 WindowTreeClient* window_tree_client,
                                 bool is_embedder)
    : MusLsiAllocator(window_port_mus, window_tree_client) {
  if (is_embedder) {
    client_surface_embedder_ =
        std::make_unique<ClientSurfaceEmbedder>(GetWindow());
  }
}

ParentAllocator::~ParentAllocator() = default;

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

void ParentAllocator::OnFrameSinkIdChanged() {
  Update(/* send_bounds_change */ false);
}

void ParentAllocator::OnInstalled() {
  // This triggers allocating a LocalSurfaceId *and* notifying the server.
  AllocateLocalSurfaceId();
}

ParentAllocator* ParentAllocator::AsParentAllocator() {
  return this;
}

void ParentAllocator::AllocateLocalSurfaceId() {
  last_surface_size_in_pixels_ = window_port_mus()->GetSizeInPixels();
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

void ParentAllocator::Update(bool send_bounds_change) {
  // If not in a bounds change, then need to update server of new
  // LocalSurfaceId.
  if (send_bounds_change) {
    const gfx::Rect& bounds = GetWindow()->bounds();
    window_tree_client()->OnWindowMusBoundsChanged(window_port_mus(), bounds,
                                                   bounds);
  }
  if (GetWindow()->IsEmbeddingClient() && client_surface_embedder_) {
    viz::SurfaceId surface_id(GetWindow()->GetFrameSinkId(),
                              GetLocalSurfaceIdAllocation().local_surface_id());
    client_surface_embedder_->SetSurfaceId(surface_id);
  }
}

// TopLevelAllocator -----------------------------------------------------------

TopLevelAllocator::TopLevelAllocator(WindowPortMus* window_port_mus,
                                     WindowTreeClient* window_tree_client)
    : MusLsiAllocator(window_port_mus, window_tree_client),
      compositor_(GetWindow()->GetHost()->compositor()) {
  DCHECK(compositor_);
  compositor_->AddObserver(this);
  // The initial allocation comes from the server, which has not been received
  // yet.
  DCHECK(!GetLocalSurfaceIdAllocation().IsValid());
}

TopLevelAllocator::~TopLevelAllocator() {
  if (compositor_)
    compositor_->RemoveObserver(this);
}

void TopLevelAllocator::UpdateLocalSurfaceIdFromParent(
    const viz::LocalSurfaceIdAllocation& local_surface_id_allocation) {
  DCHECK(local_surface_id_allocation.IsValid());
  DCHECK(compositor_);
  local_surface_id_allocation_ =
      compositor_->UpdateLocalSurfaceIdFromParent(local_surface_id_allocation);
}

TopLevelAllocator* TopLevelAllocator::AsTopLevelAllocator() {
  return this;
}

void TopLevelAllocator::AllocateLocalSurfaceId() {
  WindowTreeHostMus* window_tree_host =
      static_cast<WindowTreeHostMus*>(GetWindow()->GetHost());
  DCHECK(window_tree_host);
  if (window_tree_host->has_pending_local_surface_id_from_server()) {
    UpdateLocalSurfaceIdFromParent(
        *window_tree_host->TakePendingLocalSurfaceIdFromServer());
  }

  // This does *not* notify the server as the places that call this already
  // notify the server.
  local_surface_id_allocation_ = compositor_->RequestNewChildLocalSurfaceId();
}

viz::ScopedSurfaceIdAllocator TopLevelAllocator::GetSurfaceIdAllocator(
    base::OnceClosure allocation_task) {
  return viz::ScopedSurfaceIdAllocator(std::move(allocation_task));
}

void TopLevelAllocator::InvalidateLocalSurfaceId() {
  // It doesn't make sense to do anthing here for top-levels. Additionally this
  // should never be called for top-levels.
  NOTIMPLEMENTED();
}

void TopLevelAllocator::OnDeviceScaleFactorChanged() {
  AllocateLocalSurfaceId();
  NotifyServerOfLocalSurfaceId();
}

void TopLevelAllocator::OnDidChangeBounds(const gfx::Size& size_in_pixels,
                                          bool from_server) {
  // Processing of bounds changes for top-levels happens from WindowTreeHostMus.
}

const viz::LocalSurfaceIdAllocation&
TopLevelAllocator::GetLocalSurfaceIdAllocation() {
  return local_surface_id_allocation_;
}

void TopLevelAllocator::OnCompositingShuttingDown(ui::Compositor* compositor) {
  DCHECK_EQ(compositor_, compositor);
  compositor_->RemoveObserver(this);
  compositor_ = nullptr;
}

void TopLevelAllocator::DidGenerateLocalSurfaceIdAllocation(
    ui::Compositor* compositor,
    const viz::LocalSurfaceIdAllocation& allocation) {
  local_surface_id_allocation_ = allocation;
  // The LocalSurfaceIdAllocation needs to be applied to the Compositor as well,
  // otherwise nothing will be drawn.
  WindowTreeHost* window_tree_host = GetWindow()->GetHost();
  compositor->SetScaleAndSize(window_tree_host->device_scale_factor(),
                              window_tree_host->GetBoundsInPixels().size(),
                              local_surface_id_allocation_);
  NotifyServerOfLocalSurfaceId();
}

void TopLevelAllocator::NotifyServerOfLocalSurfaceId() {
  window_tree_client()->OnWindowTreeHostBoundsWillChange(
      static_cast<WindowTreeHostMus*>(GetWindow()->GetHost()),
      GetWindow()->GetHost()->GetBoundsInPixels());
}

// EmbeddedAllocator -----------------------------------------------------------

EmbeddedAllocator::EmbeddedAllocator(WindowPortMus* window_port_mus,
                                     WindowTreeClient* window_tree_client)
    : MusLsiAllocator(window_port_mus, window_tree_client),
      compositor_(GetWindow()->GetHost()->compositor()) {
  DCHECK(compositor_);
  compositor_->AddObserver(this);
}

EmbeddedAllocator::~EmbeddedAllocator() {
  if (compositor_)
    compositor_->RemoveObserver(this);
}

void EmbeddedAllocator::AllocateLocalSurfaceId() {
  // EmbeddedAllocators should never generate LocalSurfaceIds.
  NOTREACHED();
}

viz::ScopedSurfaceIdAllocator EmbeddedAllocator::GetSurfaceIdAllocator(
    base::OnceClosure allocation_task) {
  return viz::ScopedSurfaceIdAllocator(std::move(allocation_task));
}

void EmbeddedAllocator::InvalidateLocalSurfaceId() {
  // It doesn't make sense to do anthing here for embedded windows.
  NOTIMPLEMENTED();
}

void EmbeddedAllocator::OnDeviceScaleFactorChanged() {
  // The embedder is responsible for allocating and generating a new id.
}

void EmbeddedAllocator::OnDidChangeBounds(const gfx::Size& size_in_pixels,
                                          bool from_server) {
  // Only the server side can change the bounds.
}

const viz::LocalSurfaceIdAllocation&
EmbeddedAllocator::GetLocalSurfaceIdAllocation() {
  local_surface_id_allocation_ = compositor_->GetLocalSurfaceIdAllocation();
  return local_surface_id_allocation_;
}

void EmbeddedAllocator::OnCompositingShuttingDown(ui::Compositor* compositor) {
  DCHECK_EQ(compositor_, compositor);
  compositor_->RemoveObserver(this);
  compositor_ = nullptr;
}

void EmbeddedAllocator::DidGenerateLocalSurfaceIdAllocation(
    ui::Compositor* compositor,
    const viz::LocalSurfaceIdAllocation& allocation) {
  local_surface_id_allocation_ = allocation;
  // The LocalSurfaceIdAllocation needs to be applied to the Compositor as well,
  // otherwise nothing will be drawn.
  WindowTreeHost* window_tree_host = GetWindow()->GetHost();
  compositor->SetScaleAndSize(window_tree_host->device_scale_factor(),
                              window_tree_host->GetBoundsInPixels().size(),
                              local_surface_id_allocation_);
  NotifyServerOfLocalSurfaceId();
}

void EmbeddedAllocator::NotifyServerOfLocalSurfaceId() {
  aura::WindowTreeHostMus* window_tree_host =
      static_cast<WindowTreeHostMus*>(GetWindow()->GetHost());
  DCHECK(window_tree_host);
  DCHECK_EQ(GetWindow(), window_tree_host->window());
  window_tree_client()->tree_->UpdateLocalSurfaceIdFromChild(
      window_port_mus()->server_id(), GetLocalSurfaceIdAllocation());
}

}  // namespace aura

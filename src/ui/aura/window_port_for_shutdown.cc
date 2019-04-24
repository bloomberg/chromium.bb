// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/window_port_for_shutdown.h"

#include "cc/trees/layer_tree_frame_sink.h"
#include "ui/aura/window.h"

namespace aura {

WindowPortForShutdown::WindowPortForShutdown()
    : WindowPort(WindowPort::Type::kShutdown) {}

WindowPortForShutdown::~WindowPortForShutdown() {}

// static
void WindowPortForShutdown::Install(aura::Window* window) {
  window->port_owner_.reset(new WindowPortForShutdown);
  window->port_ = window->port_owner_.get();
}

void WindowPortForShutdown::OnPreInit(Window* window) {}

void WindowPortForShutdown::OnDeviceScaleFactorChanged(
    float old_device_scale_factor,
    float new_device_scale_factor) {}

void WindowPortForShutdown::OnWillAddChild(Window* child) {}

void WindowPortForShutdown::OnWillRemoveChild(Window* child) {}

void WindowPortForShutdown::OnWillMoveChild(size_t current_index,
                                            size_t dest_index) {}

void WindowPortForShutdown::OnVisibilityChanged(bool visible) {}

void WindowPortForShutdown::OnDidChangeBounds(const gfx::Rect& old_bounds,
                                              const gfx::Rect& new_bounds) {}

void WindowPortForShutdown::OnDidChangeTransform(
    const gfx::Transform& old_transform,
    const gfx::Transform& new_transform) {}

std::unique_ptr<ui::PropertyData> WindowPortForShutdown::OnWillChangeProperty(
    const void* key) {
  return nullptr;
}

void WindowPortForShutdown::OnPropertyChanged(
    const void* key,
    int64_t old_value,
    std::unique_ptr<ui::PropertyData> data) {}

std::unique_ptr<cc::LayerTreeFrameSink>
WindowPortForShutdown::CreateLayerTreeFrameSink() {
  return nullptr;
}

void WindowPortForShutdown::AllocateLocalSurfaceId() {}
void WindowPortForShutdown::InvalidateLocalSurfaceId() {}
void WindowPortForShutdown::UpdateLocalSurfaceIdFromEmbeddedClient(
    const viz::LocalSurfaceIdAllocation&
        embedded_client_local_surface_id_allocation) {}

viz::ScopedSurfaceIdAllocator WindowPortForShutdown::GetSurfaceIdAllocator(
    base::OnceCallback<void()> allocation_task) {
  return viz::ScopedSurfaceIdAllocator(std::move(allocation_task));
}

const viz::LocalSurfaceIdAllocation&
WindowPortForShutdown::GetLocalSurfaceIdAllocation() {
  return local_surface_id_allocation_;
}

void WindowPortForShutdown::OnEventTargetingPolicyChanged() {}

bool WindowPortForShutdown::ShouldRestackTransientChildren() {
  return true;
}

}  // namespace aura

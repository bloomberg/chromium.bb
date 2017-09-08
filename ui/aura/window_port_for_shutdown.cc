// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/window_port_for_shutdown.h"

#include "base/memory/ptr_util.h"
#include "cc/output/layer_tree_frame_sink.h"
#include "ui/aura/window.h"

namespace aura {

WindowPortForShutdown::WindowPortForShutdown() {}

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

viz::SurfaceId WindowPortForShutdown::GetSurfaceId() const {
  return viz::SurfaceId();
}

void WindowPortForShutdown::AllocateLocalSurfaceId() {}

const viz::LocalSurfaceId& WindowPortForShutdown::GetLocalSurfaceId() {
  return local_surface_id_;
}

viz::FrameSinkId WindowPortForShutdown::GetFrameSinkId() const {
  return frame_sink_id_;
}

void WindowPortForShutdown::OnWindowAddedToRootWindow() {}

void WindowPortForShutdown::OnWillRemoveWindowFromRootWindow() {}

void WindowPortForShutdown::OnEventTargetingPolicyChanged() {}

}  // namespace aura

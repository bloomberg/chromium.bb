// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/local/window_port_local.h"

#include "components/viz/service/frame_sinks/frame_sink_manager_impl.h"
#include "ui/aura/client/cursor_client.h"
#include "ui/aura/env.h"
#include "ui/aura/local/layer_tree_frame_sink_local.h"
#include "ui/aura/window.h"
#include "ui/aura/window_delegate.h"
#include "ui/base/layout.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"

namespace aura {
namespace {

class ScopedCursorHider {
 public:
  explicit ScopedCursorHider(Window* window)
      : window_(window), hid_cursor_(false) {
    if (!window_->IsRootWindow())
      return;
    const bool cursor_is_in_bounds = window_->GetBoundsInScreen().Contains(
        Env::GetInstance()->last_mouse_location());
    client::CursorClient* cursor_client = client::GetCursorClient(window_);
    if (cursor_is_in_bounds && cursor_client &&
        cursor_client->IsCursorVisible()) {
      cursor_client->HideCursor();
      hid_cursor_ = true;
    }
  }
  ~ScopedCursorHider() {
    if (!window_->IsRootWindow())
      return;

    // Update the device scale factor of the cursor client only when the last
    // mouse location is on this root window.
    if (hid_cursor_) {
      client::CursorClient* cursor_client = client::GetCursorClient(window_);
      if (cursor_client) {
        const display::Display& display =
            display::Screen::GetScreen()->GetDisplayNearestWindow(window_);
        cursor_client->SetDisplay(display);
        cursor_client->ShowCursor();
      }
    }
  }

 private:
  Window* window_;
  bool hid_cursor_;

  DISALLOW_COPY_AND_ASSIGN(ScopedCursorHider);
};

}  // namespace

WindowPortLocal::WindowPortLocal(Window* window)
    : window_(window), weak_factory_(this) {}

WindowPortLocal::~WindowPortLocal() {}

void WindowPortLocal::OnPreInit(Window* window) {}

void WindowPortLocal::OnDeviceScaleFactorChanged(float device_scale_factor) {
  if (last_device_scale_factor_ != device_scale_factor &&
      local_surface_id_.is_valid()) {
    last_device_scale_factor_ = device_scale_factor;
    local_surface_id_ = local_surface_id_allocator_.GenerateId();
    if (frame_sink_)
      frame_sink_->SetLocalSurfaceId(local_surface_id_);
  }

  ScopedCursorHider hider(window_);
  if (window_->delegate())
    window_->delegate()->OnDeviceScaleFactorChanged(device_scale_factor);
}

void WindowPortLocal::OnWillAddChild(Window* child) {}

void WindowPortLocal::OnWillRemoveChild(Window* child) {}

void WindowPortLocal::OnWillMoveChild(size_t current_index, size_t dest_index) {
}

void WindowPortLocal::OnVisibilityChanged(bool visible) {}

void WindowPortLocal::OnDidChangeBounds(const gfx::Rect& old_bounds,
                                        const gfx::Rect& new_bounds) {
  if (last_size_ != new_bounds.size() && local_surface_id_.is_valid()) {
    last_size_ = new_bounds.size();
    local_surface_id_ = local_surface_id_allocator_.GenerateId();
    if (frame_sink_)
      frame_sink_->SetLocalSurfaceId(local_surface_id_);
  }
}

void WindowPortLocal::OnDidChangeTransform(
    const gfx::Transform& old_transform,
    const gfx::Transform& new_transform) {}

std::unique_ptr<ui::PropertyData> WindowPortLocal::OnWillChangeProperty(
    const void* key) {
  return nullptr;
}

void WindowPortLocal::OnPropertyChanged(
    const void* key,
    int64_t old_value,
    std::unique_ptr<ui::PropertyData> data) {}

std::unique_ptr<cc::LayerTreeFrameSink>
WindowPortLocal::CreateLayerTreeFrameSink() {
  DCHECK(!frame_sink_id_.is_valid());
  auto* context_factory_private =
      aura::Env::GetInstance()->context_factory_private();
  frame_sink_id_ = context_factory_private->AllocateFrameSinkId();
  auto frame_sink = base::MakeUnique<LayerTreeFrameSinkLocal>(
      frame_sink_id_, context_factory_private->GetHostFrameSinkManager(),
      window_->GetName());
  frame_sink->SetSurfaceChangedCallback(base::Bind(
      &WindowPortLocal::OnSurfaceChanged, weak_factory_.GetWeakPtr()));
  frame_sink_ = frame_sink->GetWeakPtr();
  local_surface_id_ = local_surface_id_allocator_.GenerateId();
  frame_sink->SetLocalSurfaceId(local_surface_id_);
  if (window_->GetRootWindow())
    window_->layer()->GetCompositor()->AddFrameSink(frame_sink_id_);
  return std::move(frame_sink);
}

viz::SurfaceId WindowPortLocal::GetSurfaceId() const {
  return viz::SurfaceId(frame_sink_id_, local_surface_id_);
}

void WindowPortLocal::AllocateLocalSurfaceId() {
  local_surface_id_ = local_surface_id_allocator_.GenerateId();
  if (frame_sink_)
    frame_sink_->SetLocalSurfaceId(local_surface_id_);
}

const viz::LocalSurfaceId& WindowPortLocal::GetLocalSurfaceId() {
  if (!local_surface_id_.is_valid()) {
    last_device_scale_factor_ = ui::GetScaleFactorForNativeView(window_);
    last_size_ = window_->bounds().size();
    local_surface_id_ = local_surface_id_allocator_.GenerateId();
  }
  return local_surface_id_;
}

viz::FrameSinkId WindowPortLocal::GetFrameSinkId() const {
  return frame_sink_id_;
}

void WindowPortLocal::OnWindowAddedToRootWindow() {
  if (frame_sink_id_.is_valid())
    window_->layer()->GetCompositor()->AddFrameSink(frame_sink_id_);
}

void WindowPortLocal::OnWillRemoveWindowFromRootWindow() {
  if (frame_sink_id_.is_valid())
    window_->layer()->GetCompositor()->RemoveFrameSink(frame_sink_id_);
}

void WindowPortLocal::OnEventTargetingPolicyChanged() {}

void WindowPortLocal::OnSurfaceChanged(const viz::SurfaceInfo& surface_info) {
  DCHECK_EQ(surface_info.id().frame_sink_id(), frame_sink_id_);
  DCHECK_EQ(surface_info.id().local_surface_id(), local_surface_id_);
  scoped_refptr<viz::SurfaceReferenceFactory> reference_factory =
      aura::Env::GetInstance()
          ->context_factory_private()
          ->GetFrameSinkManager()
          ->surface_manager()
          ->reference_factory();
  window_->layer()->SetShowPrimarySurface(surface_info, reference_factory);
  window_->layer()->SetFallbackSurface(surface_info);
}

}  // namespace aura

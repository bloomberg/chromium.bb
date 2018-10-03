// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/wayland_output_manager.h"

#include "ui/ozone/platform/wayland/wayland_connection.h"
#include "ui/ozone/platform/wayland/wayland_output.h"

namespace ui {

namespace {
constexpr int64_t kPrimaryOutputId = 1;
}

WaylandOutputManager::WaylandOutputManager() = default;

WaylandOutputManager::~WaylandOutputManager() = default;

bool WaylandOutputManager::IsPrimaryOutputReady() const {
  return !output_list_.empty() ? output_list_.front()->is_ready() : false;
}

void WaylandOutputManager::AddWaylandOutput(wl_output* output) {
  auto wayland_output =
      std::make_unique<WaylandOutput>(++next_display_id_, output);
  WaylandOutput* wayland_output_ptr = wayland_output.get();
  output_list_.push_back(std::move(wayland_output));

  OnWaylandOutputAdded(wayland_output_ptr->output_id());

  // If WaylandScreen has already been created, the output can be initialized,
  // which results in setting up a wl_listener and getting the geometry and the
  // scaling factor from the Wayland Compositor.
  wayland_output_ptr->Initialize(this);
}

void WaylandOutputManager::RemoveWaylandOutput(wl_output* output) {
  // TODO(msisov): this requires implementation of
  // wl_registry_listener::global_remove in the WaylandConnection.
  //
  // https://crbug.com/890276
  NOTIMPLEMENTED_LOG_ONCE();
}

std::unique_ptr<WaylandScreen> WaylandOutputManager::CreateWaylandScreen() {
  auto wayland_screen = std::make_unique<WaylandScreen>();
  wayland_screen_ = wayland_screen->GetWeakPtr();

  // As long as |wl_output| sends geometry and other events asynchronously (that
  // is, the initial configuration is sent once the interface is bound), we'll
  // have to tell each output to manually inform the delegate about available
  // geometry, scale factor and etc, which will result in feeding the
  // WaylandScreen with the data through OnOutputHandleGeometry and
  // OutOutputHandleScale. All the other hot geometry and scale changes are done
  // automatically, and the |wayland_screen_| is notified immediately about the
  // changes.
  if (!output_list_.empty()) {
    for (auto& output : output_list_) {
      OnWaylandOutputAdded(output->output_id());
      output->TriggerDelegateNotification();
    }
  }

  return wayland_screen;
}

void WaylandOutputManager::OnWaylandOutputAdded(uint32_t output_id) {
  DCHECK(output_id != 0);
  if (wayland_screen_)
    wayland_screen_->OnOutputAdded(output_id, output_id == kPrimaryOutputId);
}

void WaylandOutputManager::OnWaylandOutputRemoved(uint32_t output_id) {
  if (wayland_screen_)
    wayland_screen_->OnOutputRemoved(output_id);
}

void WaylandOutputManager::OnOutputHandleMetrics(uint32_t output_id,
                                                 const gfx::Rect& new_bounds,
                                                 int32_t scale_factor) {
  if (wayland_screen_) {
    wayland_screen_->OnOutputMetricsChanged(output_id, new_bounds, scale_factor,
                                            output_id == kPrimaryOutputId);
  }
}

}  // namespace ui

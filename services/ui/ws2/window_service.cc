// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ui/ws2/window_service.h"

#include "services/ui/ws2/window_data.h"
#include "services/ui/ws2/window_service_client.h"

namespace ui {
namespace ws2 {

WindowService::WindowService(WindowServiceDelegate* delegate)
    : delegate_(delegate) {}

WindowService::~WindowService() {}

WindowData* WindowService::GetWindowDataForWindowCreateIfNecessary(
    aura::Window* window) {
  WindowData* data = WindowData::GetMayBeNull(window);
  if (data)
    return data;

  const viz::FrameSinkId frame_sink_id =
      ClientWindowId(kWindowServerClientId, next_window_id_++);
  CHECK_NE(0u, next_window_id_);
  return WindowData::Create(window, nullptr, frame_sink_id);
}

std::unique_ptr<WindowServiceClient> WindowService::CreateWindowServiceClient(
    mojom::WindowTreeClient* window_tree_client,
    bool intercepts_events) {
  const ClientSpecificId client_id = next_client_id_++;
  CHECK_NE(0u, next_client_id_);
  return std::make_unique<WindowServiceClient>(
      this, client_id, window_tree_client, intercepts_events);
}

}  // namespace ws2
}  // namespace ui

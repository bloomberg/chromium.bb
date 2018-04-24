// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ui/ws2/window_service_client_test_helper.h"

#include "services/ui/ws2/window_service_client.h"

namespace ui {
namespace ws2 {

WindowServiceClientTestHelper::WindowServiceClientTestHelper(
    WindowServiceClient* window_service_client)
    : window_service_client_(window_service_client) {}

WindowServiceClientTestHelper::~WindowServiceClientTestHelper() = default;

aura::Window* WindowServiceClientTestHelper::NewTopLevelWindow(
    Id transport_window_id) {
  base::flat_map<std::string, std::vector<uint8_t>> properties;
  const uint32_t change_id = 1u;
  window_service_client_->NewTopLevelWindow(change_id, transport_window_id,
                                            properties);
  return window_service_client_->GetWindowByClientId(
      window_service_client_->MakeClientWindowId(transport_window_id));
}

void WindowServiceClientTestHelper::SetWindowBounds(aura::Window* window,
                                                    const gfx::Rect& bounds,
                                                    uint32_t change_id) {
  base::Optional<viz::LocalSurfaceId> local_surface_id;
  window_service_client_->SetWindowBounds(
      change_id, window_service_client_->TransportIdForWindow(window), bounds,
      local_surface_id);
}

}  // namespace ws2
}  // namespace ui

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

mojom::WindowTree* WindowServiceClientTestHelper::window_tree() {
  return static_cast<mojom::WindowTree*>(window_service_client_);
}

mojom::WindowDataPtr WindowServiceClientTestHelper::WindowToWindowData(
    aura::Window* window) {
  return window_service_client_->WindowToWindowData(window);
}

aura::Window* WindowServiceClientTestHelper::NewWindow(
    Id transport_window_id,
    base::flat_map<std::string, std::vector<uint8_t>> properties) {
  const uint32_t change_id = 1u;
  window_service_client_->NewWindow(change_id, transport_window_id, properties);
  return window_service_client_->GetWindowByClientId(
      window_service_client_->MakeClientWindowId(transport_window_id));
}

void WindowServiceClientTestHelper::DeleteWindow(aura::Window* window) {
  const int change_id = 1u;
  window_service_client_->DeleteWindow(
      change_id, window_service_client_->TransportIdForWindow(window));
}

aura::Window* WindowServiceClientTestHelper::NewTopLevelWindow(
    Id transport_window_id,
    base::flat_map<std::string, std::vector<uint8_t>> properties) {
  const uint32_t change_id = 1u;
  window_service_client_->NewTopLevelWindow(change_id, transport_window_id,
                                            properties);
  return window_service_client_->GetWindowByClientId(
      window_service_client_->MakeClientWindowId(transport_window_id));
}

bool WindowServiceClientTestHelper::SetCapture(aura::Window* window) {
  return window_service_client_->SetCaptureImpl(
      ClientWindowIdForWindow(window));
}

bool WindowServiceClientTestHelper::ReleaseCapture(aura::Window* window) {
  return window_service_client_->ReleaseCaptureImpl(
      ClientWindowIdForWindow(window));
}

void WindowServiceClientTestHelper::SetWindowBounds(aura::Window* window,
                                                    const gfx::Rect& bounds,
                                                    uint32_t change_id) {
  base::Optional<viz::LocalSurfaceId> local_surface_id;
  window_service_client_->SetWindowBounds(
      change_id, window_service_client_->TransportIdForWindow(window), bounds,
      local_surface_id);
}

void WindowServiceClientTestHelper::SetClientArea(
    aura::Window* window,
    const gfx::Insets& insets,
    base::Optional<std::vector<gfx::Rect>> additional_client_areas) {
  window_service_client_->SetClientArea(
      window_service_client_->TransportIdForWindow(window), insets,
      additional_client_areas);
}

void WindowServiceClientTestHelper::SetWindowProperty(
    aura::Window* window,
    const std::string& name,
    const std::vector<uint8_t>& value,
    uint32_t change_id) {
  window_service_client_->SetWindowProperty(
      change_id, window_service_client_->TransportIdForWindow(window), name,
      value);
}

void WindowServiceClientTestHelper::SetEventTargetingPolicy(
    aura::Window* window,
    mojom::EventTargetingPolicy policy) {
  window_service_client_->SetEventTargetingPolicy(
      window_service_client_->TransportIdForWindow(window), policy);
}

ClientWindowId WindowServiceClientTestHelper::ClientWindowIdForWindow(
    aura::Window* window) {
  return window_service_client_->MakeClientWindowId(
      window_service_client_->TransportIdForWindow(window));
}

}  // namespace ws2
}  // namespace ui

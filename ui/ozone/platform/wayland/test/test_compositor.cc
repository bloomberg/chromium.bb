// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/test/test_compositor.h"

#include <wayland-server-core.h>

#include "ui/ozone/platform/wayland/test/mock_surface.h"
#include "ui/ozone/platform/wayland/test/server_object.h"

namespace wl {

namespace {

constexpr uint32_t kCompositorVersion = 4;

void CreateSurface(wl_client* client, wl_resource* resource, uint32_t id) {
  auto* compositor = GetUserDataAs<TestCompositor>(resource);
  wl_resource* surface_resource = wl_resource_create(
      client, &wl_surface_interface, wl_resource_get_version(resource), id);
  if (!surface_resource) {
    wl_client_post_no_memory(client);
    return;
  }
  SetImplementation(surface_resource, &kMockSurfaceImpl,
                    std::make_unique<MockSurface>(surface_resource));
  compositor->AddSurface(GetUserDataAs<MockSurface>(surface_resource));
}

}  // namespace

const struct wl_compositor_interface kTestCompositorImpl = {
    &CreateSurface,  // create_surface
    nullptr,         // create_region
};

TestCompositor::TestCompositor()
    : GlobalObject(&wl_compositor_interface,
                   &kTestCompositorImpl,
                   kCompositorVersion) {}

TestCompositor::~TestCompositor() {}

void TestCompositor::AddSurface(MockSurface* surface) {
  surfaces_.push_back(surface);
}

}  // namespace wl

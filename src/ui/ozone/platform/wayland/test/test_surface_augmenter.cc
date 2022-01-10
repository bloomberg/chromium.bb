// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/test/test_surface_augmenter.h"

#include <surface-augmenter-server-protocol.h>

#include "base/notreached.h"
#include "ui/ozone/platform/wayland/test/mock_surface.h"
#include "ui/ozone/platform/wayland/test/server_object.h"
#include "ui/ozone/platform/wayland/test/test_augmented_surface.h"

namespace wl {

namespace {

constexpr uint32_t kSurfaceAugmenterProtocolVersion = 2;

void CreateSolidColorBuffer(struct wl_client* client,
                            struct wl_resource* resource,
                            uint32_t id,
                            struct wl_array* color,
                            int32_t width,
                            int32_t height) {
  NOTIMPLEMENTED_LOG_ONCE();
}

void GetGetAugmentedSurface(struct wl_client* client,
                            struct wl_resource* resource,
                            uint32_t id,
                            struct wl_resource* surface) {
  auto* mock_surface = GetUserDataAs<MockSurface>(surface);
  if (mock_surface->augmented_surface()) {
    wl_resource_post_error(resource,
                           SURFACE_AUGMENTER_ERROR_AUGMENTED_SURFACE_EXISTS,
                           "surface_augmenter exists");
    return;
  }

  wl_resource* augmented_surface_resource =
      CreateResourceWithImpl<::testing::NiceMock<TestAugmentedSurface>>(
          client, &augmented_surface_interface,
          wl_resource_get_version(resource), &kTestAugmentedSurfaceImpl, id,
          surface);
  DCHECK(augmented_surface_resource);
  mock_surface->set_augmented_surface(
      GetUserDataAs<TestAugmentedSurface>(augmented_surface_resource));
}

}  // namespace

const struct surface_augmenter_interface kTestSurfaceAugmenterImpl = {
    DestroyResource,
    CreateSolidColorBuffer,
    GetGetAugmentedSurface,
};

TestSurfaceAugmenter::TestSurfaceAugmenter()
    : GlobalObject(&surface_augmenter_interface,
                   &kTestSurfaceAugmenterImpl,
                   kSurfaceAugmenterProtocolVersion) {}

TestSurfaceAugmenter::~TestSurfaceAugmenter() = default;

}  // namespace wl

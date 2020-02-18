// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_TEST_TEST_REGION_H_
#define UI_OZONE_PLATFORM_WAYLAND_TEST_TEST_REGION_H_

#include <wayland-server-protocol-core.h>

#include "third_party/skia/include/core/SkRegion.h"

namespace wl {

extern const struct wl_region_interface kTestWlRegionImpl;

using TestRegion = SkRegion;

}  // namespace wl

#endif  // UI_OZONE_PLATFORM_WAYLAND_TEST_TEST_REGION_H_

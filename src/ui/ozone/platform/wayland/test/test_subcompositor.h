// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_TEST_TEST_SUBCOMPOSITOR_H_
#define UI_OZONE_PLATFORM_WAYLAND_TEST_TEST_SUBCOMPOSITOR_H_

#include <vector>

#include "base/macros.h"
#include "ui/ozone/platform/wayland/test/global_object.h"

namespace wl {

// Manage wl_compositor object.
class TestSubCompositor : public GlobalObject {
 public:
  TestSubCompositor();
  ~TestSubCompositor() override;
  TestSubCompositor(const TestSubCompositor& rhs) = delete;
  TestSubCompositor& operator=(const TestSubCompositor& rhs) = delete;
};

}  // namespace wl

#endif  // UI_OZONE_PLATFORM_WAYLAND_TEST_TEST_SUBCOMPOSITOR_H_

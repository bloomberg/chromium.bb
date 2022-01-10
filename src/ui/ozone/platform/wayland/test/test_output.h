// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_TEST_TEST_OUTPUT_H_
#define UI_OZONE_PLATFORM_WAYLAND_TEST_TEST_OUTPUT_H_

#include <cstdint>

#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/ozone/platform/wayland/test/global_object.h"

namespace wl {

// Handle wl_output object.
class TestOutput : public GlobalObject {
 public:
  TestOutput();

  TestOutput(const TestOutput&) = delete;
  TestOutput& operator=(const TestOutput&) = delete;

  ~TestOutput() override;

  const gfx::Rect GetRect() { return rect_; }
  void SetRect(const gfx::Rect& rect);
  int32_t GetScale() const { return scale_; }
  void SetScale(int32_t factor);

  void Flush();

 protected:
  void OnBind() override;

 private:
  gfx::Rect rect_;
  int32_t scale_;

  absl::optional<gfx::Rect> pending_rect_ = absl::nullopt;
  absl::optional<int32_t> pending_scale_ = absl::nullopt;
};

}  // namespace wl

#endif  // UI_OZONE_PLATFORM_WAYLAND_TEST_TEST_OUTPUT_H_

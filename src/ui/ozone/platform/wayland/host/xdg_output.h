// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_HOST_XDG_OUTPUT_H_
#define UI_OZONE_PLATFORM_WAYLAND_HOST_XDG_OUTPUT_H_

#include <stdint.h>

#include "ui/gfx/geometry/size.h"
#include "ui/ozone/platform/wayland/common/wayland_object.h"

namespace ui {

class XDGOutput {
 public:
  explicit XDGOutput(struct zxdg_output_v1* xdg_output);
  XDGOutput(const XDGOutput&) = delete;
  XDGOutput& operator=(const XDGOutput&) = delete;
  ~XDGOutput();

  gfx::Size logical_size() const { return logical_size_; }

 private:
  static void OutputHandleLogicalPosition(void* data,
                                          struct zxdg_output_v1* zxdg_output_v1,
                                          int32_t x,
                                          int32_t y);
  static void OutputHandleLogicalSize(void* data,
                                      struct zxdg_output_v1* zxdg_output_v1,
                                      int32_t width,
                                      int32_t height);
  static void OutputHandleDone(void* data,
                               struct zxdg_output_v1* zxdg_output_v1);
  static void OutputHandleName(void* data,
                               struct zxdg_output_v1* zxdg_output_v1,
                               const char* name);
  static void OutputHandleDescription(void* data,
                                      struct zxdg_output_v1* zxdg_output_v1,
                                      const char* description);

  wl::Object<zxdg_output_v1> xdg_output_;
  gfx::Size logical_size_;
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_HOST_XDG_OUTPUT_H_

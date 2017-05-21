// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_DISPLAY_MOJO_DISPLAY_STRUCT_TRAITS_H_
#define UI_DISPLAY_MOJO_DISPLAY_STRUCT_TRAITS_H_

#include "ui/display/display.h"
#include "ui/display/mojo/display.mojom.h"
#include "ui/gfx/geometry/mojo/geometry_struct_traits.h"

namespace mojo {

template <>
struct EnumTraits<display::mojom::Rotation, display::Display::Rotation> {
  static display::mojom::Rotation ToMojom(display::Display::Rotation type);
  static bool FromMojom(display::mojom::Rotation type,
                        display::Display::Rotation* output);
};

template <>
struct EnumTraits<display::mojom::TouchSupport,
                  display::Display::TouchSupport> {
  static display::mojom::TouchSupport ToMojom(
      display::Display::TouchSupport type);
  static bool FromMojom(display::mojom::TouchSupport type,
                        display::Display::TouchSupport* output);
};

template <>
struct StructTraits<display::mojom::DisplayDataView, display::Display> {
  static int64_t id(const display::Display& display) { return display.id(); }

  static const gfx::Rect& bounds(const display::Display& display) {
    return display.bounds();
  }

  static gfx::Size size_in_pixels(const display::Display& display) {
    return display.GetSizeInPixel();
  }

  static const gfx::Rect& work_area(const display::Display& display) {
    return display.work_area();
  }

  static float device_scale_factor(const display::Display& display) {
    return display.device_scale_factor();
  }

  static display::Display::Rotation rotation(const display::Display& display) {
    return display.rotation();
  }

  static display::Display::TouchSupport touch_support(
      const display::Display& display) {
    return display.touch_support();
  }

  static const gfx::Size& maximum_cursor_size(const display::Display& display) {
    return display.maximum_cursor_size();
  }

  static int32_t color_depth(const display::Display& display) {
    return display.color_depth();
  }

  static int32_t depth_per_component(const display::Display& display) {
    return display.depth_per_component();
  }

  static bool is_monochrome(const display::Display& display) {
    return display.is_monochrome();
  }

  static bool Read(display::mojom::DisplayDataView data, display::Display* out);
};

}  // namespace mojo

#endif  // UI_DISPLAY_MOJO_DISPLAY_STRUCT_TRAITS_H_

// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_MOJOM_WAYLAND_OVERLAY_CONFIG_MOJOM_TRAITS_H_
#define UI_OZONE_PLATFORM_WAYLAND_MOJOM_WAYLAND_OVERLAY_CONFIG_MOJOM_TRAITS_H_

#include "skia/public/mojom/skcolor_mojom_traits.h"
#include "ui/gfx/mojom/gpu_fence_handle_mojom_traits.h"
#include "ui/gfx/mojom/overlay_priority_hint_mojom_traits.h"
#include "ui/gfx/mojom/overlay_transform_mojom_traits.h"
#include "ui/gfx/mojom/rrect_f_mojom_traits.h"
#include "ui/ozone/platform/wayland/common/wayland_overlay_config.h"
#include "ui/ozone/platform/wayland/mojom/wayland_overlay_config.mojom-shared.h"

namespace mojo {

template <>
struct StructTraits<wl::mojom::WaylandOverlayConfigDataView,
                    wl::WaylandOverlayConfig> {
  static int z_order(const wl::WaylandOverlayConfig& input) {
    return input.z_order;
  }

  static const gfx::OverlayTransform& transform(
      const wl::WaylandOverlayConfig& input) {
    return input.transform;
  }

  static uint32_t buffer_id(const wl::WaylandOverlayConfig& input) {
    return input.buffer_id;
  }

  static float surface_scale_factor(const wl::WaylandOverlayConfig& input) {
    return input.surface_scale_factor;
  }

  static const gfx::RectF& bounds_rect(const wl::WaylandOverlayConfig& input) {
    return input.bounds_rect;
  }

  static const gfx::RectF& crop_rect(const wl::WaylandOverlayConfig& input) {
    return input.crop_rect;
  }

  static const gfx::Rect& damage_region(const wl::WaylandOverlayConfig& input) {
    return input.damage_region;
  }

  static bool enable_blend(const wl::WaylandOverlayConfig& input) {
    return input.enable_blend;
  }

  static float opacity(const wl::WaylandOverlayConfig& input) {
    return input.opacity;
  }

  static gfx::GpuFenceHandle access_fence_handle(
      const wl::WaylandOverlayConfig& input) {
    return input.access_fence_handle.Clone();
  }

  static const gfx::OverlayPriorityHint& priority_hint(
      const wl::WaylandOverlayConfig& input) {
    return input.priority_hint;
  }

  static const gfx::RRectF& rounded_clip_bounds(
      const wl::WaylandOverlayConfig& input) {
    return input.rounded_clip_bounds;
  }

  static const absl::optional<SkColor>& background_color(
      const wl::WaylandOverlayConfig& input) {
    return input.background_color;
  }

  static bool Read(wl::mojom::WaylandOverlayConfigDataView data,
                   wl::WaylandOverlayConfig* out);
};

}  // namespace mojo

#endif  // UI_OZONE_PLATFORM_WAYLAND_MOJOM_WAYLAND_OVERLAY_CONFIG_MOJOM_TRAITS_H_

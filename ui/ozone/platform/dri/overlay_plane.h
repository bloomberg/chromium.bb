// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_DRI_OVERLAY_PLANE_H_
#define UI_OZONE_PLATFORM_DRI_OVERLAY_PLANE_H_

#include <vector>

#include "base/memory/ref_counted.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/overlay_transform.h"

namespace ui {

class ScanoutBuffer;

struct OverlayPlane;
typedef std::vector<OverlayPlane> OverlayPlaneList;

struct OverlayPlane {
  // Simpler constructor for the primary plane.
  explicit OverlayPlane(scoped_refptr<ScanoutBuffer> buffer);

  OverlayPlane(scoped_refptr<ScanoutBuffer> buffer,
               int z_order,
               gfx::OverlayTransform plane_transform,
               const gfx::Rect& display_bounds,
               const gfx::RectF& crop_rect);

  ~OverlayPlane();

  // Returns the primary plane in |overlays|.
  static const OverlayPlane& GetPrimaryPlane(const OverlayPlaneList& overlays);

  scoped_refptr<ScanoutBuffer> buffer;
  int z_order;
  gfx::OverlayTransform plane_transform;
  gfx::Rect display_bounds;
  gfx::RectF crop_rect;
  int overlay_plane;
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_DRI_OVERLAY_PLANE_H_

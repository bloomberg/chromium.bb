// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_COMMON_HIT_TEST_HIT_TEST_REGION_LIST_H_
#define COMPONENTS_VIZ_COMMON_HIT_TEST_HIT_TEST_REGION_LIST_H_

#include <vector>

#include "components/viz/common/surfaces/frame_sink_id.h"
#include "components/viz/common/viz_common_export.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/transform.h"

namespace viz {

struct HitTestRegionFlags {
  // Region maps to this surface (me).
  enum : uint32_t { kHitTestMine = 0x01 };
  // Region ignored for hit testing (transparent backgrounds & hover:none).
  enum : uint32_t { kHitTestIgnore = 0x02 };
  // Region maps to child surface (OOPIF).
  enum : uint32_t { kHitTestChildSurface = 0x04 };
  // Irregular boundary - send HitTestRequest to resolve.
  enum : uint32_t { kHitTestAsk = 0x08 };

  // TODO(varkha): Add other kHitTest* flags as necessary for other event
  // sources such as mouse-wheel, stylus or perhaps even mouse-move.

  // Hit-testing for mouse events.
  enum : uint32_t { kHitTestMouse = 0x10 };
  // Hit-testing for touch events.
  enum : uint32_t { kHitTestTouch = 0x20 };
};

struct HitTestRegion {
  // HitTestRegionFlags to indicate the type of HitTestRegion.
  uint32_t flags = 0;

  // FrameSinkId of this region.
  FrameSinkId frame_sink_id;

  // The rect of the region in the coordinate space of the embedder.
  gfx::Rect rect;

  // The transform of the region.  The transform applied to the rect
  // defines the space occupied by this region in the coordinate space of
  // the embedder.
  gfx::Transform transform;
};

struct VIZ_COMMON_EXPORT HitTestRegionList {
  HitTestRegionList();
  ~HitTestRegionList();

  HitTestRegionList(HitTestRegionList&&);
  HitTestRegionList& operator=(HitTestRegionList&&);

  // HitTestRegionFlags indicate how to handle events that match no sub-regions.
  // kHitTestMine routes un-matched events to this surface (opaque).
  // kHitTestIgnore keeps previous match in the parent (transparent).
  uint32_t flags = 0;

  // The bounds of the surface.
  gfx::Rect bounds;

  // The transform applied to all regions in this surface.
  gfx::Transform transform;

  // The list of sub-regions in front to back order.
  std::vector<HitTestRegion> regions;
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_COMMON_HIT_TEST_HIT_TEST_REGION_LIST_H_

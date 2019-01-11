// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAGE_SPATIAL_NAVIGATION_CONTROLLER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAGE_SPATIAL_NAVIGATION_CONTROLLER_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/heap/handle.h"

namespace blink {

class KeyboardEvent;
class Page;

// Encapsulates logic and state related to "spatial navigation". Spatial
// Navigation is used to move and interact with a page in a purely directional
// way, e.g. keyboard arrows.
class CORE_EXPORT SpatialNavigationController
    : public GarbageCollected<SpatialNavigationController> {
 public:
  static SpatialNavigationController* Create(Page& page);
  SpatialNavigationController(Page& page);

  bool HandleArrowKeyboardEvent(KeyboardEvent* event);

  void Trace(blink::Visitor*);

 private:
  Member<Page> page_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PAGE_SPATIAL_NAVIGATION_CONTROLLER_H_

// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_LAYER_HIT_TEST_RECTS_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_LAYER_HIT_TEST_RECTS_H_

#include "third_party/blink/renderer/platform/geometry/layout_rect.h"
#include "third_party/blink/renderer/platform/graphics/touch_action.h"
#include "third_party/blink/renderer/platform/wtf/hash_map.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

class PaintLayer;

struct TouchActionRect {
  LayoutRect rect;
  TouchAction whitelisted_touch_action;

  TouchActionRect(LayoutRect layout_rect, TouchAction action)
      : rect(layout_rect), whitelisted_touch_action(action) {}
};

typedef WTF::HashMap<const PaintLayer*, Vector<TouchActionRect>>
    LayerHitTestRects;

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_LAYER_HIT_TEST_RECTS_H_

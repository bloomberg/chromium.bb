// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LayerHitTestRects_h
#define LayerHitTestRects_h

#include "platform/geometry/LayoutRect.h"
#include "platform/graphics/TouchAction.h"
#include "platform/wtf/HashMap.h"
#include "platform/wtf/Vector.h"

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

#endif  // LayerHitTestRects_h

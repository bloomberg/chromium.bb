// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/dom/IntersectionObserverEntry.h"

#include "core/dom/Element.h"

namespace blink {

IntersectionObserverEntry::IntersectionObserverEntry(
    DOMHighResTimeStamp time,
    double intersection_ratio,
    const IntRect& bounding_client_rect,
    const IntRect* root_bounds,
    const IntRect& intersection_rect,
    bool is_intersecting,
    Element* target)
    : time_(time),
      intersection_ratio_(intersection_ratio),
      bounding_client_rect_(DOMRectReadOnly::FromIntRect(bounding_client_rect)),
      root_bounds_(root_bounds ? DOMRectReadOnly::FromIntRect(*root_bounds)
                               : nullptr),
      intersection_rect_(DOMRectReadOnly::FromIntRect(intersection_rect)),
      target_(target),
      is_intersecting_(is_intersecting)

{}

DEFINE_TRACE(IntersectionObserverEntry) {
  visitor->Trace(bounding_client_rect_);
  visitor->Trace(root_bounds_);
  visitor->Trace(intersection_rect_);
  visitor->Trace(target_);
}

}  // namespace blink

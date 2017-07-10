// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/dom/ResizeObserverEntry.h"

#include "core/dom/Element.h"
#include "core/dom/ResizeObservation.h"
#include "core/geometry/DOMRectReadOnly.h"

namespace blink {

ResizeObserverEntry::ResizeObserverEntry(Element* target,
                                         const LayoutRect& content_rect)
    : target_(target) {
  content_rect_ = DOMRectReadOnly::FromFloatRect(FloatRect(
      FloatPoint(content_rect.Location()), FloatSize(content_rect.Size())));
}

DEFINE_TRACE(ResizeObserverEntry) {
  visitor->Trace(target_);
  visitor->Trace(content_rect_);
}

}  // namespace blink

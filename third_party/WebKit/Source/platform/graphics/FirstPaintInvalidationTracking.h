// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_FIRST_PAINT_INVALIDATION_TRACKING_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_FIRST_PAINT_INVALIDATION_TRACKING_H_

#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"
#include "third_party/blink/renderer/platform/platform_export.h"

namespace blink {

class PLATFORM_EXPORT FirstPaintInvalidationTracking {
 public:
  static bool IsEnabled() {
    if (enabled_for_show_paint_rects_)
      return true;

    bool is_tracing_enabled;
    TRACE_EVENT_CATEGORY_GROUP_ENABLED(
        TRACE_DISABLED_BY_DEFAULT("blink.invalidation"), &is_tracing_enabled);
    return is_tracing_enabled;
  }

  static void SetEnabledForShowPaintRects(bool enabled) {
    enabled_for_show_paint_rects_ = enabled;
  }

 private:
  static bool enabled_for_show_paint_rects_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_FIRST_PAINT_INVALIDATION_TRACKING_H_

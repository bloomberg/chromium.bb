// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FirstPaintInvalidationTracking_h
#define FirstPaintInvalidationTracking_h

#include "platform/PlatformExport.h"
#include "platform/instrumentation/tracing/TraceEvent.h"

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

#endif  // FirstPaintInvalidationTracking_h

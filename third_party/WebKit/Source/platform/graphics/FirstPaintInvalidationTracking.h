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
  static bool isEnabled() {
    if (s_enabledForShowPaintRects)
      return true;

    bool isTracingEnabled;
    TRACE_EVENT_CATEGORY_GROUP_ENABLED(
        TRACE_DISABLED_BY_DEFAULT("blink.invalidation"), &isTracingEnabled);
    return isTracingEnabled;
  }

  static void setEnabledForShowPaintRects(bool enabled) {
    s_enabledForShowPaintRects = enabled;
  }

 private:
  static bool s_enabledForShowPaintRects;
};

}  // namespace blink

#endif  // FirstPaintInvalidationTracking_h

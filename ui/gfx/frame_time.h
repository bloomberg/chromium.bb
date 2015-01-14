// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_FRAME_TIME_H
#define UI_GFX_FRAME_TIME_H

#include "base/time/time.h"
#include "base/logging.h"

namespace gfx {

// FrameTime::Now() should be used to get timestamps with a timebase that
// is consistent across the graphics stack.
class FrameTime {
 public:
  static base::TimeTicks Now() {
    return base::TimeTicks::Now();
  }

#if defined(OS_WIN)
  static base::TimeTicks FromQPCValue(LONGLONG qpc_value) {
    DCHECK(TimestampsAreHighRes());
    return base::TimeTicks::FromQPCValue(qpc_value);
  }
#endif

  static bool TimestampsAreHighRes() {
#if defined(OS_WIN)
    return base::TimeTicks::IsHighResolution();
#else
    // TODO(miu): Mac/Linux always provide high-resolution timestamps.  Consider
    // returning base::TimeTicks::IsHighResolution() for all platforms.
    return false;
#endif
  }
};

}

#endif // UI_GFX_FRAME_TIME_H

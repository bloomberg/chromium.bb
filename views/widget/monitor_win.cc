// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "views/widget/monitor_win.h"

#include <windows.h>

#include "base/logging.h"
#include "gfx/rect.h"

namespace views {

gfx::Rect GetMonitorBoundsForRect(const gfx::Rect& rect) {
  RECT p_rect = rect.ToRECT();
  HMONITOR monitor = MonitorFromRect(&p_rect, MONITOR_DEFAULTTONEAREST);
  if (monitor) {
    MONITORINFO mi = {0};
    mi.cbSize = sizeof(mi);
    GetMonitorInfo(monitor, &mi);
    return gfx::Rect(mi.rcWork);
  }
  NOTREACHED();
  return gfx::Rect();
}

}  // namespace views

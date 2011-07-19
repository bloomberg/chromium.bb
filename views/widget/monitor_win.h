// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VIEWS_WIDGET_MONITOR_WIN_H_
#define VIEWS_WIDGET_MONITOR_WIN_H_
#pragma once

namespace gfx {
class Rect;
}

namespace views {

// Returns the bounds for the monitor that contains the largest area of
// intersection with the specified rectangle.
gfx::Rect GetMonitorBoundsForRect(const gfx::Rect& rect);

}  // namespace views

#endif  // VIEWS_WIDGET_MONITOR_WIN_H_

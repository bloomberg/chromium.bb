// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WS_WINDOW_UTILS_H_
#define SERVICES_WS_WINDOW_UTILS_H_

#include "base/component_export.h"

namespace aura {
class Window;
}

namespace gfx {
class Point;
}

namespace ws {

// Returns true if |location| is in the non-client area (or outside the bounds
// of the window). A return value of false means the location is in the client
// area.
COMPONENT_EXPORT(WINDOW_SERVICE)
bool IsLocationInNonClientArea(const aura::Window* window,
                               const gfx::Point& location);

}  // namespace ws

#endif  // SERVICES_WS_WINDOW_PROPERTIES_H_

// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_UI_WS_WINDOW_FINDER_H_
#define SERVICES_UI_WS_WINDOW_FINDER_H_

namespace gfx {
class Point;
class Transform;
}

namespace ui {
namespace ws {

class ServerWindow;

struct DeepestWindow {
  ServerWindow* window = nullptr;
  bool in_non_client_area = false;
};

// Finds the deepest visible child of |root| that should receive an event at
// |location|. |location| is in the coordinate space of |root_window|. The
// |window| field in the returned structure is set to the child window. If no
// valid child window is found |window| is set to null.
DeepestWindow FindDeepestVisibleWindowForEvents(ServerWindow* root_window,
                                                const gfx::Point& location);

// Retrieve the transform to the provided |window|'s coordinate space from the
// root.
gfx::Transform GetTransformToWindow(ServerWindow* window);

}  // namespace ws
}  // namespace ui

#endif  // SERVICES_UI_WS_WINDOW_FINDER_H_

// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_UI_WS_SERVER_WINDOW_DELEGATE_H_
#define SERVICES_UI_WS_SERVER_WINDOW_DELEGATE_H_

#include <memory>

#include "cc/ipc/display_compositor.mojom.h"
#include "services/ui/public/interfaces/mus_constants.mojom.h"
#include "services/ui/public/interfaces/window_tree.mojom.h"

namespace ui {

namespace ws {

class ServerWindow;

class ServerWindowDelegate {
 public:
  // Returns a display compositor interface pointer. There is only one
  // DisplayCompositor running in the system.
  virtual cc::mojom::DisplayCompositor* GetDisplayCompositor() = 0;

  // Returns the root of the window tree to which this |window| is attached.
  // Returns null if this window is not attached up through to a root window.
  virtual ServerWindow* GetRootWindow(const ServerWindow* window) = 0;

 protected:
  virtual ~ServerWindowDelegate() {}
};

}  // namespace ws

}  // namespace ui

#endif  // SERVICES_UI_WS_SERVER_WINDOW_DELEGATE_H_

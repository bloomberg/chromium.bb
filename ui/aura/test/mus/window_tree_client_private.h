// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_TEST_MUS_WINDOW_TREE_CLIENT_PRIVATE_H_
#define UI_AURA_TEST_MUS_WINDOW_TREE_CLIENT_PRIVATE_H_

#include <stdint.h>

#include <memory>

#include "base/macros.h"
#include "services/ui/public/interfaces/window_tree_constants.mojom.h"
#include "ui/aura/mus/mus_types.h"

namespace display {
class Display;
}

namespace ui {
class Event;

namespace mojom {
class WindowTree;
}
}

namespace aura {

class Window;
class WindowTreeClient;
class WindowTreeHostMus;

// Use to access implementation details of WindowTreeClient.
class WindowTreeClientPrivate {
 public:
  explicit WindowTreeClientPrivate(WindowTreeClient* tree_client_impl);
  explicit WindowTreeClientPrivate(Window* window);
  ~WindowTreeClientPrivate();

  // Calls OnEmbed() on the WindowTreeClient.
  void OnEmbed(ui::mojom::WindowTree* window_tree);

  WindowTreeHostMus* CallWmNewDisplayAdded(const display::Display& display);
  WindowTreeHostMus* CallWmNewDisplayAdded(const display::Display& display,
                                           ui::mojom::WindowDataPtr root_data,
                                           bool parent_drawn);

  // Pretends that |event| has been received from the window server.
  void CallOnWindowInputEvent(Window* window, std::unique_ptr<ui::Event> event);

  void CallOnCaptureChanged(Window* new_capture, Window* old_capture);

  // Sets the WindowTree and client id.
  void SetTreeAndClientId(ui::mojom::WindowTree* window_tree,
                          ClientSpecificId client_id);

  bool HasPointerWatcher();

 private:
  WindowTreeClient* tree_client_impl_;
  uint16_t next_window_id_ = 1u;

  DISALLOW_COPY_AND_ASSIGN(WindowTreeClientPrivate);
};

}  // namespace ui

#endif  // UI_AURA_TEST_MUS_WINDOW_TREE_CLIENT_PRIVATE_H_

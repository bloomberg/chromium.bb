// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_UI_PUBLIC_CPP_TESTS_WINDOW_TREE_CLIENT_PRIVATE_H_
#define SERVICES_UI_PUBLIC_CPP_TESTS_WINDOW_TREE_CLIENT_PRIVATE_H_

#include <stdint.h>

#include <memory>

#include "base/macros.h"
#include "services/ui/common/types.h"

namespace display {
class Display;
}

namespace ui {
class Event;
}

namespace ui {
namespace mojom {
class WindowTree;
}

class Window;
class WindowTreeClient;

// Use to access implementation details of WindowTreeClient.
class WindowTreeClientPrivate {
 public:
  explicit WindowTreeClientPrivate(WindowTreeClient* tree_client_impl);
  explicit WindowTreeClientPrivate(Window* window);
  ~WindowTreeClientPrivate();

  // Calls OnEmbed() on the WindowTreeClient.
  void OnEmbed(mojom::WindowTree* window_tree);

  void CallWmNewDisplayAdded(const display::Display& display);

  // Pretends that |event| has been received from the window server.
  void CallOnWindowInputEvent(Window* window, std::unique_ptr<ui::Event> event);

  void CallOnCaptureChanged(Window* new_capture, Window* old_capture);

  // Sets the WindowTree and client id.
  void SetTreeAndClientId(mojom::WindowTree* window_tree,
                          ClientSpecificId client_id);

  bool HasPointerWatcher();

 private:
  WindowTreeClient* tree_client_impl_;
  uint16_t next_window_id_ = 1u;

  DISALLOW_COPY_AND_ASSIGN(WindowTreeClientPrivate);
};

}  // namespace ui

#endif  // SERVICES_UI_PUBLIC_CPP_TESTS_WINDOW_TREE_CLIENT_PRIVATE_H_

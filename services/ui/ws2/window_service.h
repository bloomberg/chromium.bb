// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_UI_WS2_WINDOW_SERVICE_H_
#define SERVICES_UI_WS2_WINDOW_SERVICE_H_

#include <memory>

#include "base/component_export.h"
#include "base/macros.h"
#include "services/ui/ws2/ids.h"

namespace aura {
class Window;
}

namespace ui {

namespace mojom {
class WindowTreeClient;
}

namespace ws2 {

class WindowData;
class WindowServiceClient;
class WindowServiceDelegate;

// WindowService is the entry point into providing an implementation of
// the ui::mojom::WindowTree related mojoms on top of an aura Window hierarchy.
// A WindowServiceClient is created for each client. Typically you'll
// also create a WindowTreeFactory to handle incoming
// mojom::WindowTreeFactoryRequests.
class COMPONENT_EXPORT(WINDOW_SERVICE) WindowService {
 public:
  explicit WindowService(WindowServiceDelegate* delegate);
  ~WindowService();

  // Gets the WindowData for |window|, creating if necessary.
  WindowData* GetWindowDataForWindowCreateIfNecessary(aura::Window* window);

  // Creates a new WindowServiceClient, caller must call one of the Init()
  // functions on the returned object.
  std::unique_ptr<WindowServiceClient> CreateWindowServiceClient(
      mojom::WindowTreeClient* window_tree_client,
      bool intercepts_events);

  WindowServiceDelegate* delegate() { return delegate_; }

 private:
  // Id for the next WindowServiceClient.
  ClientSpecificId next_client_id_ = kWindowServerClientId + 1;

  // Id used for the next window created locally that is exposed to clients.
  ClientSpecificId next_window_id_ = 1;

  WindowServiceDelegate* delegate_;

  DISALLOW_COPY_AND_ASSIGN(WindowService);
};

}  // namespace ws2
}  // namespace ui

#endif  // SERVICES_UI_WS2_WINDOW_SERVICE_H_

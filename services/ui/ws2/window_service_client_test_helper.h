// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_UI_WS2_WINDOW_SERVICE_CLIENT_TEST_HELPER_H_
#define SERVICES_UI_WS2_WINDOW_SERVICE_CLIENT_TEST_HELPER_H_

#include "base/macros.h"
#include "services/ui/ws2/ids.h"

namespace aura {
class Window;
}

namespace gfx {
class Rect;
}

namespace ui {
namespace ws2 {

class WindowServiceClient;

// Used for accessing private members of WindowServiceClient in tests.
class WindowServiceClientTestHelper {
 public:
  explicit WindowServiceClientTestHelper(
      WindowServiceClient* window_service_client);
  ~WindowServiceClientTestHelper();

  aura::Window* NewTopLevelWindow(Id transport_window_id);
  void SetWindowBounds(aura::Window* window,
                       const gfx::Rect& bounds,
                       uint32_t change_id = 1);

 private:
  WindowServiceClient* window_service_client_;

  DISALLOW_COPY_AND_ASSIGN(WindowServiceClientTestHelper);
};

}  // namespace ws2
}  // namespace ui

#endif  // SERVICES_UI_WS2_WINDOW_SERVICE_CLIENT_TEST_HELPER_H_

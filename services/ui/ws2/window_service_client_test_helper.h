// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_UI_WS2_WINDOW_SERVICE_CLIENT_TEST_HELPER_H_
#define SERVICES_UI_WS2_WINDOW_SERVICE_CLIENT_TEST_HELPER_H_

#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/macros.h"
#include "services/ui/public/interfaces/window_tree_constants.mojom.h"
#include "services/ui/ws2/ids.h"

namespace aura {
class Window;
}

namespace gfx {
class Rect;
}

namespace ui {

namespace mojom {
class WindowTree;

enum class EventTargetingPolicy;
}  // namespace mojom

namespace ws2 {

class WindowServiceClient;

// Used for accessing private members of WindowServiceClient in tests.
class WindowServiceClientTestHelper {
 public:
  explicit WindowServiceClientTestHelper(
      WindowServiceClient* window_service_client);
  ~WindowServiceClientTestHelper();

  mojom::WindowTree* window_tree();

  mojom::WindowDataPtr WindowToWindowData(aura::Window* window);

  aura::Window* NewWindow(
      Id transport_window_id,
      base::flat_map<std::string, std::vector<uint8_t>> properties = {});
  aura::Window* NewTopLevelWindow(
      Id transport_window_id,
      base::flat_map<std::string, std::vector<uint8_t>> properties = {});
  void SetWindowBounds(aura::Window* window,
                       const gfx::Rect& bounds,
                       uint32_t change_id = 1);
  void SetWindowProperty(aura::Window* window,
                         const std::string& name,
                         const std::vector<uint8_t>& value,
                         uint32_t change_id = 1);
  void SetEventTargetingPolicy(aura::Window* window,
                               mojom::EventTargetingPolicy policy);

 private:
  WindowServiceClient* window_service_client_;

  DISALLOW_COPY_AND_ASSIGN(WindowServiceClientTestHelper);
};

}  // namespace ws2
}  // namespace ui

#endif  // SERVICES_UI_WS2_WINDOW_SERVICE_CLIENT_TEST_HELPER_H_

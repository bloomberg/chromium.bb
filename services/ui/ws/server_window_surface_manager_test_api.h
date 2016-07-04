// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_UI_WS_SERVER_WINDOW_SURFACE_MANAGER_TEST_API_H_
#define SERVICES_UI_WS_SERVER_WINDOW_SURFACE_MANAGER_TEST_API_H_

#include "base/macros.h"
#include "services/ui/ws/server_window_surface_manager.h"

namespace ui {
namespace ws {

class ServerWindow;

// Use to poke at the internals of ServerWindowSurfaceManager.
class ServerWindowSurfaceManagerTestApi {
 public:
  explicit ServerWindowSurfaceManagerTestApi(
      ServerWindowSurfaceManager* manager);
  ~ServerWindowSurfaceManagerTestApi();

  void CreateEmptyDefaultSurface();
  void DestroyDefaultSurface();

 private:
  ServerWindowSurfaceManager* manager_;

  DISALLOW_COPY_AND_ASSIGN(ServerWindowSurfaceManagerTestApi);
};

// Use to make |window| a target for events.
void EnableHitTest(ServerWindow* window);
void DisableHitTest(ServerWindow* window);

}  // namespace ws
}  // namespace ui

#endif  // SERVICES_UI_WS_SERVER_WINDOW_SURFACE_MANAGER_TEST_API_H_

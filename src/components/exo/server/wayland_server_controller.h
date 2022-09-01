// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_SERVER_WAYLAND_SERVER_CONTROLLER_H_
#define COMPONENTS_EXO_SERVER_WAYLAND_SERVER_CONTROLLER_H_

#include <memory>

#include "base/containers/flat_map.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/weak_ptr.h"
#include "components/exo/capabilities.h"
#include "components/exo/display.h"
#include "components/exo/wayland/server.h"

namespace exo {

namespace wayland {
class Server;
}  // namespace wayland

class DataExchangeDelegate;
class InputMethodSurfaceManager;
class NotificationSurfaceManager;
class ToastSurfaceManager;
class WMHelper;

class WaylandServerController {
 public:
  static std::unique_ptr<WaylandServerController> CreateForArcIfNecessary(
      std::unique_ptr<DataExchangeDelegate> data_exchange_delegate);

  // Creates WaylandServerController. Returns null if controller should not be
  // created.
  static std::unique_ptr<WaylandServerController> CreateIfNecessary(
      std::unique_ptr<DataExchangeDelegate> data_exchange_delegate,
      std::unique_ptr<NotificationSurfaceManager> notification_surface_manager,
      std::unique_ptr<InputMethodSurfaceManager> input_method_surface_manager,
      std::unique_ptr<ToastSurfaceManager> toast_surface_manager);

  // Returns a handle to the global-singletone instance of the server
  // controller.
  static WaylandServerController* Get();

  WaylandServerController(const WaylandServerController&) = delete;
  WaylandServerController& operator=(const WaylandServerController&) = delete;

  ~WaylandServerController();

  InputMethodSurfaceManager* input_method_surface_manager() {
    return display_->input_method_surface_manager();
  }

  WaylandServerController(
      std::unique_ptr<DataExchangeDelegate> data_exchange_delegate,
      std::unique_ptr<NotificationSurfaceManager> notification_surface_manager,
      std::unique_ptr<InputMethodSurfaceManager> input_method_surface_manager,
      std::unique_ptr<ToastSurfaceManager> toast_surface_manager);

  // Creates a wayland server with the given set of |capabilities|. Invokes
  // |callback| with the success flag indicating whether the request
  // succeeded/failed. If successful, the server and its |capabilities| will
  // persist until DeleteServer() is called.
  void CreateServer(std::unique_ptr<Capabilities> capabilities,
                    wayland::Server::StartCallback callback);

  // Removes the wayland server with a socket at |path|. This server, along with
  // its capabilities, will be deleted, and wayland clients will no longer be
  // able to connect to it.
  void DeleteServer(const base::FilePath& path);

 private:
  void OnStarted(std::unique_ptr<wayland::Server> server,
                 wayland::Server::StartCallback callback,
                 bool success,
                 const base::FilePath& path);

  std::unique_ptr<WMHelper> wm_helper_;
  std::unique_ptr<Display> display_;
  base::flat_map<base::FilePath, std::unique_ptr<wayland::Server>> servers_;
  base::WeakPtrFactory<WaylandServerController> weak_factory_{this};
};

}  // namespace exo

#endif  // COMPONENTS_EXO_SERVER_WAYLAND_SERVER_CONTROLLER_H_

// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_UI_SERVICE_H_
#define SERVICES_UI_SERVICE_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "services/shell/public/cpp/interface_factory.h"
#include "services/shell/public/cpp/service.h"
#include "services/shell/public/cpp/service_runner.h"
#include "services/tracing/public/cpp/provider.h"
#include "services/ui/input_devices/input_device_server.h"
#include "services/ui/public/interfaces/accessibility_manager.mojom.h"
#include "services/ui/public/interfaces/clipboard.mojom.h"
#include "services/ui/public/interfaces/display.mojom.h"
#include "services/ui/public/interfaces/gpu.mojom.h"
#include "services/ui/public/interfaces/gpu_service.mojom.h"
#include "services/ui/public/interfaces/user_access_manager.mojom.h"
#include "services/ui/public/interfaces/user_activity_monitor.mojom.h"
#include "services/ui/public/interfaces/window_manager_window_tree_factory.mojom.h"
#include "services/ui/public/interfaces/window_server_test.mojom.h"
#include "services/ui/public/interfaces/window_tree.mojom.h"
#include "services/ui/public/interfaces/window_tree_host.mojom.h"
#include "services/ui/ws/platform_display_init_params.h"
#include "services/ui/ws/touch_controller.h"
#include "services/ui/ws/user_id.h"
#include "services/ui/ws/window_server_delegate.h"

#if defined(USE_OZONE)
#include "ui/ozone/public/client_native_pixmap_factory.h"
#endif

namespace gfx {
class Rect;
}

namespace shell {
class Connector;
}

namespace ui {
class PlatformEventSource;
}

namespace ui {

namespace display {
class PlatformScreen;
}

namespace ws {
class ForwardingWindowManager;
class WindowServer;
}

class Service
    : public shell::Service,
      public ws::WindowServerDelegate,
      public shell::InterfaceFactory<mojom::AccessibilityManager>,
      public shell::InterfaceFactory<mojom::Clipboard>,
      public shell::InterfaceFactory<mojom::DisplayManager>,
      public shell::InterfaceFactory<mojom::GpuService>,
      public shell::InterfaceFactory<mojom::UserAccessManager>,
      public shell::InterfaceFactory<mojom::UserActivityMonitor>,
      public shell::InterfaceFactory<mojom::WindowManagerWindowTreeFactory>,
      public shell::InterfaceFactory<mojom::WindowTreeFactory>,
      public shell::InterfaceFactory<mojom::WindowTreeHostFactory>,
      public shell::InterfaceFactory<mojom::WindowServerTest> {
 public:
  Service();
  ~Service() override;

 private:
  // Holds InterfaceRequests received before the first WindowTreeHost Display
  // has been established.
  struct PendingRequest;
  struct UserState;

  using UserIdToUserState = std::map<ws::UserId, std::unique_ptr<UserState>>;

  void InitializeResources(shell::Connector* connector);

  // Returns the user specific state for the user id of |remote_identity|.
  // Service owns the return value.
  // TODO(sky): if we allow removal of user ids then we need to close anything
  // associated with the user (all incoming pipes...) on removal.
  UserState* GetUserState(const shell::Identity& remote_identity);

  void AddUserIfNecessary(const shell::Identity& remote_identity);

  // shell::Service:
  void OnStart(const shell::Identity& identity) override;
  bool OnConnect(shell::Connection* connection) override;

  // WindowServerDelegate:
  void OnFirstDisplayReady() override;
  void OnNoMoreDisplays() override;
  bool IsTestConfig() const override;
  void CreateDefaultDisplays() override;

  // shell::InterfaceFactory<mojom::AccessibilityManager> implementation.
  void Create(const shell::Identity& remote_identity,
              mojom::AccessibilityManagerRequest request) override;

  // shell::InterfaceFactory<mojom::Clipboard> implementation.
  void Create(const shell::Identity& remote_identity,
              mojom::ClipboardRequest request) override;

  // shell::InterfaceFactory<mojom::DisplayManager> implementation.
  void Create(const shell::Identity& remote_identity,
              mojom::DisplayManagerRequest request) override;

  // shell::InterfaceFactory<mojom::GpuService> implementation.
  void Create(const shell::Identity& remote_identity,
              mojom::GpuServiceRequest request) override;

  // shell::InterfaceFactory<mojom::UserAccessManager> implementation.
  void Create(const shell::Identity& remote_identity,
              mojom::UserAccessManagerRequest request) override;

  // shell::InterfaceFactory<mojom::UserActivityMonitor> implementation.
  void Create(const shell::Identity& remote_identity,
              mojom::UserActivityMonitorRequest request) override;

  // shell::InterfaceFactory<mojom::WindowManagerWindowTreeFactory>
  // implementation.
  void Create(const shell::Identity& remote_identity,
              mojom::WindowManagerWindowTreeFactoryRequest request) override;

  // shell::InterfaceFactory<mojom::WindowTreeFactory>:
  void Create(const shell::Identity& remote_identity,
              mojom::WindowTreeFactoryRequest request) override;

  // shell::InterfaceFactory<mojom::WindowTreeHostFactory>:
  void Create(const shell::Identity& remote_identity,
              mojom::WindowTreeHostFactoryRequest request) override;

  // shell::InterfaceFactory<mojom::WindowServerTest> implementation.
  void Create(const shell::Identity& remote_identity,
              mojom::WindowServerTestRequest request) override;

  // Callback for display configuration. |id| is the identifying token for the
  // configured display that will identify a specific physical display across
  // configuration changes. |bounds| is the bounds of the display in screen
  // coordinates.
  void OnCreatedPhysicalDisplay(int64_t id, const gfx::Rect& bounds);

  ws::PlatformDisplayInitParams platform_display_init_params_;
  std::unique_ptr<ws::WindowServer> window_server_;
  std::unique_ptr<ui::PlatformEventSource> event_source_;
  tracing::Provider tracing_;
  using PendingRequests = std::vector<std::unique_ptr<PendingRequest>>;
  PendingRequests pending_requests_;

  UserIdToUserState user_id_to_user_state_;

  // Provides input-device information via Mojo IPC.
  InputDeviceServer input_device_server_;

  bool test_config_;
#if defined(USE_OZONE)
  std::unique_ptr<ui::ClientNativePixmapFactory> client_native_pixmap_factory_;
#endif

  std::unique_ptr<display::PlatformScreen> platform_screen_;
  std::unique_ptr<ws::TouchController> touch_controller_;

  base::WeakPtrFactory<Service> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(Service);
};

}  // namespace ui

#endif  // SERVICES_UI_SERVICE_H_

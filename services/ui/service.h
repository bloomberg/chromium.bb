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
#include "components/discardable_memory/public/interfaces/discardable_shared_memory_manager.mojom.h"
#include "services/service_manager/public/cpp/binder_registry.h"
#include "services/service_manager/public/cpp/service.h"
#include "services/service_manager/public/cpp/service_runner.h"
#include "services/ui/ime/ime_driver_bridge.h"
#include "services/ui/ime/ime_registrar_impl.h"
#include "services/ui/input_devices/input_device_server.h"
#include "services/ui/public/interfaces/accessibility_manager.mojom.h"
#include "services/ui/public/interfaces/clipboard.mojom.h"
#include "services/ui/public/interfaces/display_manager.mojom.h"
#include "services/ui/public/interfaces/gpu.mojom.h"
#include "services/ui/public/interfaces/ime/ime.mojom.h"
#include "services/ui/public/interfaces/user_access_manager.mojom.h"
#include "services/ui/public/interfaces/user_activity_monitor.mojom.h"
#include "services/ui/public/interfaces/window_manager_window_tree_factory.mojom.h"
#include "services/ui/public/interfaces/window_server_test.mojom.h"
#include "services/ui/public/interfaces/window_tree.mojom.h"
#include "services/ui/public/interfaces/window_tree_host.mojom.h"
#include "services/ui/ws/user_id.h"
#include "services/ui/ws/window_server_delegate.h"

#if defined(USE_OZONE)
#include "ui/ozone/public/client_native_pixmap_factory_ozone.h"
#endif

namespace discardable_memory {
class DiscardableSharedMemoryManager;
}

namespace display {
class ScreenManager;
}

namespace service_manager {
class Connector;
class Identity;
}

namespace ui {

class InputDeviceController;
class PlatformEventSource;

namespace ws {
class WindowServer;
}

class Service : public service_manager::Service,
                public ws::WindowServerDelegate {
 public:
  Service();
  ~Service() override;

 private:
  // Holds InterfaceRequests received before the first WindowTreeHost Display
  // has been established.
  struct PendingRequest;
  struct UserState;

  using UserIdToUserState = std::map<ws::UserId, std::unique_ptr<UserState>>;

  // Attempts to initialize the resource bundle. Returns true if successful,
  // otherwise false if resources cannot be loaded.
  bool InitializeResources(service_manager::Connector* connector);

  // Returns the user specific state for the user id of |remote_identity|.
  // Service owns the return value.
  // TODO(sky): if we allow removal of user ids then we need to close anything
  // associated with the user (all incoming pipes...) on removal.
  UserState* GetUserState(const service_manager::Identity& remote_identity);

  void AddUserIfNecessary(const service_manager::Identity& remote_identity);

  // service_manager::Service:
  void OnStart() override;
  void OnBindInterface(const service_manager::BindSourceInfo& source_info,
                       const std::string& interface_name,
                       mojo::ScopedMessagePipeHandle interface_pipe) override;

  // WindowServerDelegate:
  void StartDisplayInit() override;
  void OnFirstDisplayReady() override;
  void OnNoMoreDisplays() override;
  bool IsTestConfig() const override;
  void OnWillCreateTreeForWindowManager(
      bool automatically_create_display_roots) override;

  void BindAccessibilityManagerRequest(
      const service_manager::BindSourceInfo& source_info,
      mojom::AccessibilityManagerRequest request);

  void BindClipboardRequest(const service_manager::BindSourceInfo& source_info,
                            mojom::ClipboardRequest request);

  void BindDisplayManagerRequest(
      const service_manager::BindSourceInfo& source_info,
      mojom::DisplayManagerRequest request);

  void BindGpuRequest(const service_manager::BindSourceInfo& source_info,
                      mojom::GpuRequest request);

  void BindIMERegistrarRequest(
      const service_manager::BindSourceInfo& source_info,
      mojom::IMERegistrarRequest request);

  void BindIMEDriverRequest(const service_manager::BindSourceInfo& source_info,
                            mojom::IMEDriverRequest request);

  void BindUserAccessManagerRequest(
      const service_manager::BindSourceInfo& source_info,
      mojom::UserAccessManagerRequest request);

  void BindUserActivityMonitorRequest(
      const service_manager::BindSourceInfo& source_info,
      mojom::UserActivityMonitorRequest request);

  void BindWindowManagerWindowTreeFactoryRequest(
      const service_manager::BindSourceInfo& source_info,
      mojom::WindowManagerWindowTreeFactoryRequest request);

  void BindWindowTreeFactoryRequest(
      const service_manager::BindSourceInfo& source_info,
      mojom::WindowTreeFactoryRequest request);

  void BindWindowTreeHostFactoryRequest(
      const service_manager::BindSourceInfo& source_info,
      mojom::WindowTreeHostFactoryRequest request);

  void BindDiscardableSharedMemoryManagerRequest(
      const service_manager::BindSourceInfo& source_info,
      discardable_memory::mojom::DiscardableSharedMemoryManagerRequest request);

  void BindWindowServerTestRequest(
      const service_manager::BindSourceInfo& source_info,
      mojom::WindowServerTestRequest request);

  std::unique_ptr<ws::WindowServer> window_server_;
  std::unique_ptr<ui::PlatformEventSource> event_source_;
  using PendingRequests = std::vector<std::unique_ptr<PendingRequest>>;
  PendingRequests pending_requests_;

  UserIdToUserState user_id_to_user_state_;

  // Provides input-device information via Mojo IPC. Registers Mojo interfaces
  // and must outlive |registry_|.
  InputDeviceServer input_device_server_;

  bool test_config_;
#if defined(USE_OZONE)
  std::unique_ptr<gfx::ClientNativePixmapFactory> client_native_pixmap_factory_;
#if defined(OS_CHROMEOS)
  std::unique_ptr<InputDeviceController> input_device_controller_;
#endif
#endif

  // Manages display hardware and handles display management. May register Mojo
  // interfaces and must outlive |registry_|.
  std::unique_ptr<display::ScreenManager> screen_manager_;

  IMERegistrarImpl ime_registrar_;
  IMEDriverBridge ime_driver_;

  std::unique_ptr<discardable_memory::DiscardableSharedMemoryManager>
      discardable_shared_memory_manager_;

  service_manager::BinderRegistry registry_;

  // Set to true in StartDisplayInit().
  bool is_gpu_ready_ = false;

  DISALLOW_COPY_AND_ASSIGN(Service);
};

}  // namespace ui

#endif  // SERVICES_UI_SERVICE_H_

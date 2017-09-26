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
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
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
#include "services/ui/public/interfaces/remote_event_dispatcher.mojom.h"
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

class ImageCursorsSet;
class InputDeviceController;
class PlatformEventSource;

namespace ws {
class ThreadedImageCursorsFactory;
class WindowServer;
}

class Service : public service_manager::Service,
                public ws::WindowServerDelegate {
 public:
  // Contains the configuration necessary to run the UI Service inside the
  // Window Manager's process.
  struct InProcessConfig {
    InProcessConfig();
    ~InProcessConfig();

    // Can be used to load resources.
    scoped_refptr<base::SingleThreadTaskRunner> resource_runner = nullptr;

    // Can only be de-referenced on |resource_runner_|.
    base::WeakPtr<ImageCursorsSet> image_cursors_set_weak_ptr = nullptr;

    // If null Service creates a DiscardableSharedMemoryManager.
    discardable_memory::DiscardableSharedMemoryManager* memory_manager =
        nullptr;

   private:
    DISALLOW_COPY_AND_ASSIGN(InProcessConfig);
  };

  // |config| should be null when UI Service runs in it's own separate process,
  // as opposed to inside the Window Manager's process.
  explicit Service(const InProcessConfig* config = nullptr);
  ~Service() override;

  // Call if the ui::Service is being run as a standalone process.
  void set_running_standalone(bool value) { running_standalone_ = value; }

 private:
  // Holds InterfaceRequests received before the first WindowTreeHost Display
  // has been established.
  struct PendingRequest;
  struct UserState;

  using UserIdToUserState = std::map<ws::UserId, std::unique_ptr<UserState>>;

  bool is_in_process() const { return is_in_process_; }

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
  ws::ThreadedImageCursorsFactory* GetThreadedImageCursorsFactory() override;

  void BindAccessibilityManagerRequest(
      mojom::AccessibilityManagerRequest request,
      const service_manager::BindSourceInfo& source_info);

  void BindClipboardRequest(mojom::ClipboardRequest request,
                            const service_manager::BindSourceInfo& source_info);

  void BindDisplayManagerRequest(
      mojom::DisplayManagerRequest request,
      const service_manager::BindSourceInfo& source_info);

  void BindGpuRequest(mojom::GpuRequest request);

  void BindIMERegistrarRequest(mojom::IMERegistrarRequest request);

  void BindIMEDriverRequest(mojom::IMEDriverRequest request);

  void BindUserAccessManagerRequest(mojom::UserAccessManagerRequest request);

  void BindUserActivityMonitorRequest(
      mojom::UserActivityMonitorRequest request,
      const service_manager::BindSourceInfo& source_info);

  void BindWindowManagerWindowTreeFactoryRequest(
      mojom::WindowManagerWindowTreeFactoryRequest request,
      const service_manager::BindSourceInfo& source_info);

  void BindWindowTreeFactoryRequest(
      mojom::WindowTreeFactoryRequest request,
      const service_manager::BindSourceInfo& source_info);

  void BindWindowTreeHostFactoryRequest(
      mojom::WindowTreeHostFactoryRequest request,
      const service_manager::BindSourceInfo& source_info);

  void BindDiscardableSharedMemoryManagerRequest(
      discardable_memory::mojom::DiscardableSharedMemoryManagerRequest request,
      const service_manager::BindSourceInfo& source_info);

  void BindWindowServerTestRequest(mojom::WindowServerTestRequest request);

  void BindRemoteEventDispatcherRequest(
      mojom::RemoteEventDispatcherRequest request);

  std::unique_ptr<ws::WindowServer> window_server_;
  std::unique_ptr<PlatformEventSource> event_source_;
  using PendingRequests = std::vector<std::unique_ptr<PendingRequest>>;
  PendingRequests pending_requests_;

  UserIdToUserState user_id_to_user_state_;

  // Provides input-device information via Mojo IPC. Registers Mojo interfaces
  // and must outlive |registry_|.
  InputDeviceServer input_device_server_;

  // True if the UI Service runs inside WM's process, false if it runs inside
  // its own process.
  const bool is_in_process_;

  std::unique_ptr<ws::ThreadedImageCursorsFactory>
      threaded_image_cursors_factory_;

  bool test_config_;

#if defined(USE_OZONE)
  std::unique_ptr<gfx::ClientNativePixmapFactory> client_native_pixmap_factory_;
#endif

#if defined(OS_CHROMEOS)
  std::unique_ptr<InputDeviceController> input_device_controller_;
#endif

  // Manages display hardware and handles display management. May register Mojo
  // interfaces and must outlive |registry_|.
  std::unique_ptr<display::ScreenManager> screen_manager_;

  IMERegistrarImpl ime_registrar_;
  IMEDriverBridge ime_driver_;

  discardable_memory::DiscardableSharedMemoryManager*
      discardable_shared_memory_manager_;

  // non-null if this created the DiscardableSharedMemoryManager. Null when
  // running in-process.
  std::unique_ptr<discardable_memory::DiscardableSharedMemoryManager>
      owned_discardable_shared_memory_manager_;

  service_manager::BinderRegistryWithArgs<
      const service_manager::BindSourceInfo&>
      registry_with_source_info_;
  service_manager::BinderRegistry registry_;

  // Set to true in StartDisplayInit().
  bool is_gpu_ready_ = false;

  bool in_destructor_ = false;

  bool running_standalone_ = false;

  DISALLOW_COPY_AND_ASSIGN(Service);
};

}  // namespace ui

#endif  // SERVICES_UI_SERVICE_H_

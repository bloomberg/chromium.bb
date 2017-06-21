// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ui/service.h"

#include <set>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/memory/ptr_util.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/threading/platform_thread.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "components/discardable_memory/service/discardable_shared_memory_manager.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "services/catalog/public/cpp/resource_loader.h"
#include "services/catalog/public/interfaces/constants.mojom.h"
#include "services/service_manager/public/c/main.h"
#include "services/service_manager/public/cpp/connector.h"
#include "services/service_manager/public/cpp/service_context.h"
#include "services/ui/clipboard/clipboard_impl.h"
#include "services/ui/common/switches.h"
#include "services/ui/display/screen_manager.h"
#include "services/ui/ime/ime_driver_bridge.h"
#include "services/ui/ime/ime_registrar_impl.h"
#include "services/ui/ws/accessibility_manager.h"
#include "services/ui/ws/display_binding.h"
#include "services/ui/ws/display_creation_config.h"
#include "services/ui/ws/display_manager.h"
#include "services/ui/ws/frame_sink_manager_client_binding.h"
#include "services/ui/ws/gpu_host.h"
#include "services/ui/ws/user_activity_monitor.h"
#include "services/ui/ws/user_display_manager.h"
#include "services/ui/ws/window_server.h"
#include "services/ui/ws/window_server_test_impl.h"
#include "services/ui/ws/window_tree.h"
#include "services/ui/ws/window_tree_binding.h"
#include "services/ui/ws/window_tree_factory.h"
#include "services/ui/ws/window_tree_host_factory.h"
#include "ui/base/platform_window_defaults.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/ui_base_paths.h"
#include "ui/events/event_switches.h"
#include "ui/events/platform/platform_event_source.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gl/gl_surface.h"

#if defined(USE_X11)
#include <X11/Xlib.h>
#include "ui/base/x/x11_util.h"  // nogncheck
#include "ui/platform_window/x11/x11_window.h"
#elif defined(USE_OZONE)
#include "services/ui/display/screen_manager_forwarding.h"
#include "ui/events/ozone/layout/keyboard_layout_engine.h"
#include "ui/events/ozone/layout/keyboard_layout_engine_manager.h"
#include "ui/ozone/public/ozone_platform.h"
#endif

#if defined(OS_CHROMEOS) && defined(USE_OZONE)
#include "services/ui/public/cpp/input_devices/input_device_controller.h"
#endif

using mojo::InterfaceRequest;
using ui::mojom::WindowServerTest;
using ui::mojom::WindowTreeHostFactory;

namespace ui {

namespace {

const char kResourceFileStrings[] = "mus_app_resources_strings.pak";
const char kResourceFile100[] = "mus_app_resources_100.pak";
const char kResourceFile200[] = "mus_app_resources_200.pak";

}  // namespace

// TODO(sky): this is a pretty typical pattern, make it easier to do.
struct Service::PendingRequest {
  service_manager::BindSourceInfo source_info;
  std::unique_ptr<mojom::WindowTreeFactoryRequest> wtf_request;
  std::unique_ptr<mojom::DisplayManagerRequest> dm_request;
};

struct Service::UserState {
  std::unique_ptr<clipboard::ClipboardImpl> clipboard;
  std::unique_ptr<ws::AccessibilityManager> accessibility;
  std::unique_ptr<ws::WindowTreeHostFactory> window_tree_host_factory;
};

Service::Service() : test_config_(false), ime_registrar_(&ime_driver_) {}

Service::~Service() {
  // Destroy |window_server_| first, since it depends on |event_source_|.
  // WindowServer (or more correctly its Displays) may have state that needs to
  // be destroyed before GpuState as well.
  window_server_.reset();

#if defined(USE_OZONE)
#if defined(OS_CHROMEOS)
  // InputDeviceController uses ozone.
  input_device_controller_.reset();
#endif

  OzonePlatform::Shutdown();
#endif
}

bool Service::InitializeResources(service_manager::Connector* connector) {
  if (ui::ResourceBundle::HasSharedInstance())
    return true;

  std::set<std::string> resource_paths;
  resource_paths.insert(kResourceFileStrings);
  resource_paths.insert(kResourceFile100);
  resource_paths.insert(kResourceFile200);

  catalog::ResourceLoader loader;
  filesystem::mojom::DirectoryPtr directory;
  connector->BindInterface(catalog::mojom::kServiceName, &directory);
  if (!loader.OpenFiles(std::move(directory), resource_paths)) {
    LOG(ERROR) << "Service failed to open resource files.";
    return false;
  }

  ui::RegisterPathProvider();

  // Initialize resource bundle with 1x and 2x cursor bitmaps.
  ui::ResourceBundle::InitSharedInstanceWithPakFileRegion(
      loader.TakeFile(kResourceFileStrings),
      base::MemoryMappedFile::Region::kWholeFile);
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  rb.AddDataPackFromFile(loader.TakeFile(kResourceFile100),
                         ui::SCALE_FACTOR_100P);
  rb.AddDataPackFromFile(loader.TakeFile(kResourceFile200),
                         ui::SCALE_FACTOR_200P);
  return true;
}

Service::UserState* Service::GetUserState(
    const service_manager::Identity& remote_identity) {
  const ws::UserId& user_id = remote_identity.user_id();
  auto it = user_id_to_user_state_.find(user_id);
  if (it != user_id_to_user_state_.end())
    return it->second.get();
  user_id_to_user_state_[user_id] = base::WrapUnique(new UserState);
  return user_id_to_user_state_[user_id].get();
}

void Service::AddUserIfNecessary(
    const service_manager::Identity& remote_identity) {
  window_server_->user_id_tracker()->AddUserId(remote_identity.user_id());
}

void Service::OnStart() {
  base::PlatformThread::SetName("mus");
  TRACE_EVENT0("mus", "Service::Initialize started");

  test_config_ = base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kUseTestConfig);
#if defined(USE_X11)
  XInitThreads();
  ui::SetDefaultX11ErrorHandlers();
#endif

  if (test_config_)
    ui::test::EnableTestConfigForPlatformWindows();

  // If resources are unavailable do not complete start-up.
  if (!InitializeResources(context()->connector())) {
    context()->QuitNow();
    return;
  }

#if defined(USE_OZONE)
  // The ozone platform can provide its own event source. So initialize the
  // platform before creating the default event source.
  // Because GL libraries need to be initialized before entering the sandbox,
  // in MUS, |InitializeForUI| will load the GL libraries.
  ui::OzonePlatform::InitParams params;
  params.connector = context()->connector();
  params.single_process = false;
  ui::OzonePlatform::InitializeForUI(params);

  // Assume a client will change the layout to an appropriate configuration.
  ui::KeyboardLayoutEngineManager::GetKeyboardLayoutEngine()
      ->SetCurrentLayoutByName("us");
  client_native_pixmap_factory_ = ui::CreateClientNativePixmapFactoryOzone();
  gfx::ClientNativePixmapFactory::SetInstance(
      client_native_pixmap_factory_.get());

  DCHECK(gfx::ClientNativePixmapFactory::GetInstance());

#if defined(OS_CHROMEOS)
  input_device_controller_ = base::MakeUnique<InputDeviceController>();
  input_device_controller_->AddInterface(&registry_);
#endif
#endif

// TODO(rjkroege): Enter sandbox here before we start threads in GpuState
// http://crbug.com/584532

#if !defined(OS_ANDROID)
  event_source_ = ui::PlatformEventSource::CreateDefault();
#endif

  // This needs to happen after DeviceDataManager has been constructed. That
  // happens either during OzonePlatform or PlatformEventSource initialization,
  // so keep this line below both of those.
  input_device_server_.RegisterAsObserver();

  window_server_.reset(new ws::WindowServer(this));
  std::unique_ptr<ws::GpuHost> gpu_host =
      base::MakeUnique<ws::DefaultGpuHost>(window_server_.get());
  std::unique_ptr<ws::FrameSinkManagerClientBinding> frame_sink_manager =
      base::MakeUnique<ws::FrameSinkManagerClientBinding>(window_server_.get(),
                                                          gpu_host.get());
  window_server_->SetGpuHost(std::move(gpu_host));
  window_server_->SetFrameSinkManager(std::move(frame_sink_manager));

  ime_driver_.Init(context()->connector(), test_config_);

  discardable_shared_memory_manager_ =
      base::MakeUnique<discardable_memory::DiscardableSharedMemoryManager>();

  registry_.AddInterface<mojom::AccessibilityManager>(base::Bind(
      &Service::BindAccessibilityManagerRequest, base::Unretained(this)));
  registry_.AddInterface<mojom::Clipboard>(
      base::Bind(&Service::BindClipboardRequest, base::Unretained(this)));
  registry_.AddInterface<mojom::DisplayManager>(
      base::Bind(&Service::BindDisplayManagerRequest, base::Unretained(this)));
  registry_.AddInterface<mojom::Gpu>(
      base::Bind(&Service::BindGpuRequest, base::Unretained(this)));
  registry_.AddInterface<mojom::IMERegistrar>(
      base::Bind(&Service::BindIMERegistrarRequest, base::Unretained(this)));
  registry_.AddInterface<mojom::IMEDriver>(
      base::Bind(&Service::BindIMEDriverRequest, base::Unretained(this)));
  registry_.AddInterface<mojom::UserAccessManager>(base::Bind(
      &Service::BindUserAccessManagerRequest, base::Unretained(this)));
  registry_.AddInterface<mojom::UserActivityMonitor>(base::Bind(
      &Service::BindUserActivityMonitorRequest, base::Unretained(this)));
  registry_.AddInterface<WindowTreeHostFactory>(base::Bind(
      &Service::BindWindowTreeHostFactoryRequest, base::Unretained(this)));
  registry_.AddInterface<mojom::WindowManagerWindowTreeFactory>(
      base::Bind(&Service::BindWindowManagerWindowTreeFactoryRequest,
                 base::Unretained(this)));
  registry_.AddInterface<mojom::WindowTreeFactory>(base::Bind(
      &Service::BindWindowTreeFactoryRequest, base::Unretained(this)));
  registry_
      .AddInterface<discardable_memory::mojom::DiscardableSharedMemoryManager>(
          base::Bind(&Service::BindDiscardableSharedMemoryManagerRequest,
                     base::Unretained(this)));
  if (test_config_) {
    registry_.AddInterface<WindowServerTest>(base::Bind(
        &Service::BindWindowServerTestRequest, base::Unretained(this)));
  }

  // On non-Linux platforms there will be no DeviceDataManager instance and no
  // purpose in adding the Mojo interface to connect to.
  if (input_device_server_.IsRegisteredAsObserver())
    input_device_server_.AddInterface(&registry_);

#if defined(USE_OZONE)
  ui::OzonePlatform::GetInstance()->AddInterfaces(&registry_);
#endif
}

void Service::OnBindInterface(
    const service_manager::BindSourceInfo& source_info,
    const std::string& interface_name,
    mojo::ScopedMessagePipeHandle interface_pipe) {
  registry_.BindInterface(source_info, interface_name,
                          std::move(interface_pipe));
}

void Service::StartDisplayInit() {
  DCHECK(!is_gpu_ready_);  // This should only be called once.
  is_gpu_ready_ = true;
  if (screen_manager_)
    screen_manager_->Init(window_server_->display_manager());
}

void Service::OnFirstDisplayReady() {
  PendingRequests requests;
  requests.swap(pending_requests_);
  for (auto& request : requests) {
    if (request->wtf_request) {
      BindWindowTreeFactoryRequest(request->source_info,
                                   std::move(*request->wtf_request));
    } else {
      BindDisplayManagerRequest(request->source_info,
                                std::move(*request->dm_request));
    }
  }
}

void Service::OnNoMoreDisplays() {
  // We may get here from the destructor, in which case there is no messageloop.
  if (base::RunLoop::IsRunningOnCurrentThread())
    base::MessageLoop::current()->QuitWhenIdle();
}

bool Service::IsTestConfig() const {
  return test_config_;
}

void Service::OnWillCreateTreeForWindowManager(
    bool automatically_create_display_roots) {
  if (window_server_->display_creation_config() !=
      ws::DisplayCreationConfig::UNKNOWN) {
    return;
  }

  DVLOG(3) << "OnWillCreateTreeForWindowManager "
           << automatically_create_display_roots;
  ws::DisplayCreationConfig config = automatically_create_display_roots
                                         ? ws::DisplayCreationConfig::AUTOMATIC
                                         : ws::DisplayCreationConfig::MANUAL;
  window_server_->SetDisplayCreationConfig(config);
  if (window_server_->display_creation_config() ==
      ws::DisplayCreationConfig::MANUAL) {
#if defined(USE_OZONE) && defined(OS_CHROMEOS)
    screen_manager_ = base::MakeUnique<display::ScreenManagerForwarding>();
#else
    CHECK(false);
#endif
  } else {
    screen_manager_ = display::ScreenManager::Create();
  }
  screen_manager_->AddInterfaces(&registry_);
  if (is_gpu_ready_)
    screen_manager_->Init(window_server_->display_manager());
}

void Service::BindAccessibilityManagerRequest(
    const service_manager::BindSourceInfo& source_info,
    mojom::AccessibilityManagerRequest request) {
  UserState* user_state = GetUserState(source_info.identity);
  if (!user_state->accessibility) {
    const ws::UserId& user_id = source_info.identity.user_id();
    user_state->accessibility.reset(
        new ws::AccessibilityManager(window_server_.get(), user_id));
  }
  user_state->accessibility->Bind(std::move(request));
}

void Service::BindClipboardRequest(
    const service_manager::BindSourceInfo& source_info,
    mojom::ClipboardRequest request) {
  UserState* user_state = GetUserState(source_info.identity);
  if (!user_state->clipboard)
    user_state->clipboard.reset(new clipboard::ClipboardImpl);
  user_state->clipboard->AddBinding(std::move(request));
}

void Service::BindDisplayManagerRequest(
    const service_manager::BindSourceInfo& source_info,
    mojom::DisplayManagerRequest request) {
  // Wait for the DisplayManager to be configured before binding display
  // requests. Otherwise the client sees no displays.
  if (!window_server_->display_manager()->IsReady()) {
    std::unique_ptr<PendingRequest> pending_request(new PendingRequest);
    pending_request->source_info = source_info;
    pending_request->dm_request.reset(
        new mojom::DisplayManagerRequest(std::move(request)));
    pending_requests_.push_back(std::move(pending_request));
    return;
  }
  window_server_->display_manager()
      ->GetUserDisplayManager(source_info.identity.user_id())
      ->AddDisplayManagerBinding(std::move(request));
}

void Service::BindGpuRequest(const service_manager::BindSourceInfo& source_info,
                             mojom::GpuRequest request) {
  window_server_->gpu_host()->Add(std::move(request));
}

void Service::BindIMERegistrarRequest(
    const service_manager::BindSourceInfo& source_info,
    mojom::IMERegistrarRequest request) {
  ime_registrar_.AddBinding(std::move(request));
}

void Service::BindIMEDriverRequest(
    const service_manager::BindSourceInfo& source_info,
    mojom::IMEDriverRequest request) {
  ime_driver_.AddBinding(std::move(request));
}

void Service::BindUserAccessManagerRequest(
    const service_manager::BindSourceInfo& source_info,
    mojom::UserAccessManagerRequest request) {
  window_server_->user_id_tracker()->Bind(std::move(request));
}

void Service::BindUserActivityMonitorRequest(
    const service_manager::BindSourceInfo& source_info,
    mojom::UserActivityMonitorRequest request) {
  AddUserIfNecessary(source_info.identity);
  const ws::UserId& user_id = source_info.identity.user_id();
  window_server_->GetUserActivityMonitorForUser(user_id)->Add(
      std::move(request));
}

void Service::BindWindowManagerWindowTreeFactoryRequest(
    const service_manager::BindSourceInfo& source_info,
    mojom::WindowManagerWindowTreeFactoryRequest request) {
  AddUserIfNecessary(source_info.identity);
  window_server_->window_manager_window_tree_factory_set()->Add(
      source_info.identity.user_id(), std::move(request));
}

void Service::BindWindowTreeFactoryRequest(
    const service_manager::BindSourceInfo& source_info,
    mojom::WindowTreeFactoryRequest request) {
  AddUserIfNecessary(source_info.identity);
  if (!window_server_->display_manager()->IsReady()) {
    std::unique_ptr<PendingRequest> pending_request(new PendingRequest);
    pending_request->source_info = source_info;
    pending_request->wtf_request.reset(
        new mojom::WindowTreeFactoryRequest(std::move(request)));
    pending_requests_.push_back(std::move(pending_request));
    return;
  }
  AddUserIfNecessary(source_info.identity);
  mojo::MakeStrongBinding(
      base::MakeUnique<ws::WindowTreeFactory>(window_server_.get(),
                                              source_info.identity.user_id(),
                                              source_info.identity.name()),
      std::move(request));
}

void Service::BindWindowTreeHostFactoryRequest(
    const service_manager::BindSourceInfo& source_info,
    mojom::WindowTreeHostFactoryRequest request) {
  UserState* user_state = GetUserState(source_info.identity);
  if (!user_state->window_tree_host_factory) {
    user_state->window_tree_host_factory.reset(new ws::WindowTreeHostFactory(
        window_server_.get(), source_info.identity.user_id()));
  }
  user_state->window_tree_host_factory->AddBinding(std::move(request));
}

void Service::BindDiscardableSharedMemoryManagerRequest(
    const service_manager::BindSourceInfo& source_info,
    discardable_memory::mojom::DiscardableSharedMemoryManagerRequest request) {
  discardable_shared_memory_manager_->Bind(source_info, std::move(request));
}

void Service::BindWindowServerTestRequest(
    const service_manager::BindSourceInfo& source_info,
    mojom::WindowServerTestRequest request) {
  if (!test_config_)
    return;
  mojo::MakeStrongBinding(
      base::MakeUnique<ws::WindowServerTestImpl>(window_server_.get()),
      std::move(request));
}


}  // namespace ui

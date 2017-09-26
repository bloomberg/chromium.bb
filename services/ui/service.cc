// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ui/service.h"

#include <set>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/memory/ptr_util.h"
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
#include "services/ui/common/image_cursors_set.h"
#include "services/ui/common/switches.h"
#include "services/ui/display/screen_manager.h"
#include "services/ui/ime/ime_driver_bridge.h"
#include "services/ui/ime/ime_registrar_impl.h"
#include "services/ui/ws/accessibility_manager.h"
#include "services/ui/ws/display_binding.h"
#include "services/ui/ws/display_creation_config.h"
#include "services/ui/ws/display_manager.h"
#include "services/ui/ws/gpu_host.h"
#include "services/ui/ws/remote_event_dispatcher.h"
#include "services/ui/ws/threaded_image_cursors.h"
#include "services/ui/ws/threaded_image_cursors_factory.h"
#include "services/ui/ws/user_activity_monitor.h"
#include "services/ui/ws/user_display_manager.h"
#include "services/ui/ws/window_server.h"
#include "services/ui/ws/window_server_test_impl.h"
#include "services/ui/ws/window_tree.h"
#include "services/ui/ws/window_tree_binding.h"
#include "services/ui/ws/window_tree_factory.h"
#include "services/ui/ws/window_tree_host_factory.h"
#include "ui/base/cursor/image_cursors.h"
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

#if defined(OS_CHROMEOS)
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

class ThreadedImageCursorsFactoryImpl : public ws::ThreadedImageCursorsFactory {
 public:
  // Uses the same InProcessConfig as the UI Service. |config| will be null when
  // the UI Service runs in it's own separate process as opposed to the WM's
  // process.
  explicit ThreadedImageCursorsFactoryImpl(
      const Service::InProcessConfig* config) {
    if (config) {
      resource_runner_ = config->resource_runner;
      image_cursors_set_weak_ptr_ = config->image_cursors_set_weak_ptr;
      DCHECK(resource_runner_);
    }
  }

  ~ThreadedImageCursorsFactoryImpl() override {}

  // ws::ThreadedImageCursorsFactory:
  std::unique_ptr<ws::ThreadedImageCursors> CreateCursors() override {
    // |resource_runner_| will not be initialized if and only if UI Service runs
    // in it's own separate process. In this case we can (lazily) initialize it
    // to the current thread (i.e. the UI Services's thread). We also initialize
    // the local |image_cursors_set_| and make |image_cursors_set_weak_ptr_|
    // point to it.
    if (!resource_runner_) {
      resource_runner_ = base::ThreadTaskRunnerHandle::Get();
      image_cursors_set_ = base::MakeUnique<ui::ImageCursorsSet>();
      image_cursors_set_weak_ptr_ = image_cursors_set_->GetWeakPtr();
    }
    return base::MakeUnique<ws::ThreadedImageCursors>(
        resource_runner_, image_cursors_set_weak_ptr_);
  }

 private:
  scoped_refptr<base::SingleThreadTaskRunner> resource_runner_;
  base::WeakPtr<ui::ImageCursorsSet> image_cursors_set_weak_ptr_;

  // Used when UI Service doesn't run inside WM's process.
  std::unique_ptr<ui::ImageCursorsSet> image_cursors_set_;

  DISALLOW_COPY_AND_ASSIGN(ThreadedImageCursorsFactoryImpl);
};

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

Service::InProcessConfig::InProcessConfig() = default;

Service::InProcessConfig::~InProcessConfig() = default;

Service::Service(const InProcessConfig* config)
    : is_in_process_(config != nullptr),
      threaded_image_cursors_factory_(
          base::MakeUnique<ThreadedImageCursorsFactoryImpl>(config)),
      test_config_(false),
      ime_registrar_(&ime_driver_),
      discardable_shared_memory_manager_(config ? config->memory_manager
                                                : nullptr) {}

Service::~Service() {
  in_destructor_ = true;

  // Destroy |window_server_| first, since it depends on |event_source_|.
  // WindowServer (or more correctly its Displays) may have state that needs to
  // be destroyed before GpuState as well.
  window_server_.reset();

  // Must be destroyed before calling OzonePlatform::Shutdown().
  threaded_image_cursors_factory_.reset();

#if defined(OS_CHROMEOS)
  // InputDeviceController uses ozone.
  input_device_controller_.reset();
#endif

#if defined(USE_OZONE)
  OzonePlatform::Shutdown();
#endif
}

bool Service::InitializeResources(service_manager::Connector* connector) {
  if (is_in_process() || ui::ResourceBundle::HasSharedInstance())
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

  if (running_standalone_)
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
  params.single_process = true;
  ui::OzonePlatform::InitializeForUI(params);

  // Assume a client will change the layout to an appropriate configuration.
  ui::KeyboardLayoutEngineManager::GetKeyboardLayoutEngine()
      ->SetCurrentLayoutByName("us");

  if (!is_in_process()) {
    client_native_pixmap_factory_ = ui::CreateClientNativePixmapFactoryOzone();
    gfx::ClientNativePixmapFactory::SetInstance(
        client_native_pixmap_factory_.get());
  }

  DCHECK(gfx::ClientNativePixmapFactory::GetInstance());
#endif

#if defined(OS_CHROMEOS)
  input_device_controller_ = base::MakeUnique<InputDeviceController>();
  input_device_controller_->AddInterface(&registry_);
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

  window_server_ = base::MakeUnique<ws::WindowServer>(this);
  std::unique_ptr<ws::GpuHost> gpu_host =
      base::MakeUnique<ws::DefaultGpuHost>(window_server_.get());
  window_server_->SetGpuHost(std::move(gpu_host));

  ime_driver_.Init(context()->connector(), test_config_);

  if (!discardable_shared_memory_manager_) {
    owned_discardable_shared_memory_manager_ =
        base::MakeUnique<discardable_memory::DiscardableSharedMemoryManager>();
    discardable_shared_memory_manager_ =
        owned_discardable_shared_memory_manager_.get();
  }
  registry_with_source_info_.AddInterface<mojom::AccessibilityManager>(
      base::Bind(&Service::BindAccessibilityManagerRequest,
                 base::Unretained(this)));
  registry_with_source_info_.AddInterface<mojom::Clipboard>(
      base::Bind(&Service::BindClipboardRequest, base::Unretained(this)));
  registry_with_source_info_.AddInterface<mojom::DisplayManager>(
      base::Bind(&Service::BindDisplayManagerRequest, base::Unretained(this)));
  registry_.AddInterface<mojom::Gpu>(
      base::Bind(&Service::BindGpuRequest, base::Unretained(this)));
  registry_.AddInterface<mojom::IMERegistrar>(
      base::Bind(&Service::BindIMERegistrarRequest, base::Unretained(this)));
  registry_.AddInterface<mojom::IMEDriver>(
      base::Bind(&Service::BindIMEDriverRequest, base::Unretained(this)));
  registry_.AddInterface<mojom::UserAccessManager>(base::Bind(
      &Service::BindUserAccessManagerRequest, base::Unretained(this)));
  registry_with_source_info_.AddInterface<mojom::UserActivityMonitor>(
      base::Bind(&Service::BindUserActivityMonitorRequest,
                 base::Unretained(this)));
  registry_with_source_info_.AddInterface<WindowTreeHostFactory>(base::Bind(
      &Service::BindWindowTreeHostFactoryRequest, base::Unretained(this)));
  registry_with_source_info_
      .AddInterface<mojom::WindowManagerWindowTreeFactory>(
          base::Bind(&Service::BindWindowManagerWindowTreeFactoryRequest,
                     base::Unretained(this)));
  registry_with_source_info_.AddInterface<mojom::WindowTreeFactory>(base::Bind(
      &Service::BindWindowTreeFactoryRequest, base::Unretained(this)));
  registry_with_source_info_
      .AddInterface<discardable_memory::mojom::DiscardableSharedMemoryManager>(
          base::Bind(&Service::BindDiscardableSharedMemoryManagerRequest,
                     base::Unretained(this)));
  if (test_config_) {
    registry_.AddInterface<WindowServerTest>(base::Bind(
        &Service::BindWindowServerTestRequest, base::Unretained(this)));
  }
  registry_.AddInterface<mojom::RemoteEventDispatcher>(base::Bind(
      &Service::BindRemoteEventDispatcherRequest, base::Unretained(this)));

  // On non-Linux platforms there will be no DeviceDataManager instance and no
  // purpose in adding the Mojo interface to connect to.
  if (input_device_server_.IsRegisteredAsObserver())
    input_device_server_.AddInterface(&registry_with_source_info_);

#if defined(USE_OZONE)
  ui::OzonePlatform::GetInstance()->AddInterfaces(&registry_with_source_info_);
#endif
}

void Service::OnBindInterface(
    const service_manager::BindSourceInfo& source_info,
    const std::string& interface_name,
    mojo::ScopedMessagePipeHandle interface_pipe) {
  if (!registry_with_source_info_.TryBindInterface(
          interface_name, &interface_pipe, source_info)) {
    registry_.BindInterface(interface_name, std::move(interface_pipe));
  }
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
      BindWindowTreeFactoryRequest(std::move(*request->wtf_request),
                                   request->source_info);
    } else {
      BindDisplayManagerRequest(std::move(*request->dm_request),
                                request->source_info);
    }
  }
}

void Service::OnNoMoreDisplays() {
  // We may get here from the destructor. Don't try to use RequestQuit() when
  // that happens as ServiceContext DCHECKs in this case.
  if (in_destructor_)
    return;

  DCHECK(context());
  context()->RequestQuit();
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
#if defined(OS_CHROMEOS)
    display::ScreenManagerForwarding::Mode mode =
        is_in_process() ? display::ScreenManagerForwarding::Mode::IN_WM_PROCESS
                        : display::ScreenManagerForwarding::Mode::OWN_PROCESS;
    screen_manager_ = base::MakeUnique<display::ScreenManagerForwarding>(mode);
#else
    CHECK(false);
#endif
  } else {
    screen_manager_ = display::ScreenManager::Create();
  }
  screen_manager_->AddInterfaces(&registry_with_source_info_);
  if (is_gpu_ready_)
    screen_manager_->Init(window_server_->display_manager());
}

ws::ThreadedImageCursorsFactory* Service::GetThreadedImageCursorsFactory() {
  return threaded_image_cursors_factory_.get();
}

void Service::BindAccessibilityManagerRequest(
    mojom::AccessibilityManagerRequest request,
    const service_manager::BindSourceInfo& source_info) {
  UserState* user_state = GetUserState(source_info.identity);
  if (!user_state->accessibility) {
    const ws::UserId& user_id = source_info.identity.user_id();
    user_state->accessibility.reset(
        new ws::AccessibilityManager(window_server_.get(), user_id));
  }
  user_state->accessibility->Bind(std::move(request));
}

void Service::BindClipboardRequest(
    mojom::ClipboardRequest request,
    const service_manager::BindSourceInfo& source_info) {
  UserState* user_state = GetUserState(source_info.identity);
  if (!user_state->clipboard)
    user_state->clipboard.reset(new clipboard::ClipboardImpl);
  user_state->clipboard->AddBinding(std::move(request));
}

void Service::BindDisplayManagerRequest(
    mojom::DisplayManagerRequest request,
    const service_manager::BindSourceInfo& source_info) {
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

void Service::BindGpuRequest(mojom::GpuRequest request) {
  window_server_->gpu_host()->Add(std::move(request));
}

void Service::BindIMERegistrarRequest(mojom::IMERegistrarRequest request) {
  ime_registrar_.AddBinding(std::move(request));
}

void Service::BindIMEDriverRequest(mojom::IMEDriverRequest request) {
  ime_driver_.AddBinding(std::move(request));
}

void Service::BindUserAccessManagerRequest(
    mojom::UserAccessManagerRequest request) {
  window_server_->user_id_tracker()->Bind(std::move(request));
}

void Service::BindUserActivityMonitorRequest(
    mojom::UserActivityMonitorRequest request,
    const service_manager::BindSourceInfo& source_info) {
  AddUserIfNecessary(source_info.identity);
  const ws::UserId& user_id = source_info.identity.user_id();
  window_server_->GetUserActivityMonitorForUser(user_id)->Add(
      std::move(request));
}

void Service::BindWindowManagerWindowTreeFactoryRequest(
    mojom::WindowManagerWindowTreeFactoryRequest request,
    const service_manager::BindSourceInfo& source_info) {
  AddUserIfNecessary(source_info.identity);
  window_server_->window_manager_window_tree_factory_set()->Add(
      source_info.identity.user_id(), std::move(request));
}

void Service::BindWindowTreeFactoryRequest(
    mojom::WindowTreeFactoryRequest request,
    const service_manager::BindSourceInfo& source_info) {
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
    mojom::WindowTreeHostFactoryRequest request,
    const service_manager::BindSourceInfo& source_info) {
  UserState* user_state = GetUserState(source_info.identity);
  if (!user_state->window_tree_host_factory) {
    user_state->window_tree_host_factory.reset(new ws::WindowTreeHostFactory(
        window_server_.get(), source_info.identity.user_id()));
  }
  user_state->window_tree_host_factory->AddBinding(std::move(request));
}

void Service::BindDiscardableSharedMemoryManagerRequest(
    discardable_memory::mojom::DiscardableSharedMemoryManagerRequest request,
    const service_manager::BindSourceInfo& source_info) {
  discardable_shared_memory_manager_->Bind(std::move(request), source_info);
}

void Service::BindWindowServerTestRequest(
    mojom::WindowServerTestRequest request) {
  if (!test_config_)
    return;
  mojo::MakeStrongBinding(
      base::MakeUnique<ws::WindowServerTestImpl>(window_server_.get()),
      std::move(request));
}

void Service::BindRemoteEventDispatcherRequest(
    mojom::RemoteEventDispatcherRequest request) {
  mojo::MakeStrongBinding(
      base::MakeUnique<ws::RemoteEventDispatcherImpl>(window_server_.get()),
      std::move(request));
}

}  // namespace ui

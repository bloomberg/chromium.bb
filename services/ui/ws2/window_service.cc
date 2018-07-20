// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ui/ws2/window_service.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/single_thread_task_runner.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "services/service_manager/public/cpp/bind_source_info.h"
#include "services/ui/common/switches.h"
#include "services/ui/public/interfaces/event_injector.mojom.h"
#include "services/ui/ws2/gpu_interface_provider.h"
#include "services/ui/ws2/remoting_event_injector.h"
#include "services/ui/ws2/screen_provider.h"
#include "services/ui/ws2/server_window.h"
#include "services/ui/ws2/user_activity_monitor.h"
#include "services/ui/ws2/window_server_test_impl.h"
#include "services/ui/ws2/window_service_delegate.h"
#include "services/ui/ws2/window_tree.h"
#include "services/ui/ws2/window_tree_factory.h"
#include "ui/aura/env.h"
#include "ui/base/mojo/clipboard_host.h"
#include "ui/wm/core/shadow_types.h"

namespace ui {
namespace ws2 {

namespace {

// A placeholder to prevent test crashes on unbound requests.
void BindEventInjectorRequest(ui::mojom::EventInjectorRequest request) {
  NOTIMPLEMENTED_LOG_ONCE();
}

}  // namespace

WindowService::WindowService(
    WindowServiceDelegate* delegate,
    std::unique_ptr<GpuInterfaceProvider> gpu_interface_provider,
    aura::client::FocusClient* focus_client,
    bool decrement_client_ids)
    : delegate_(delegate),
      gpu_interface_provider_(std::move(gpu_interface_provider)),
      screen_provider_(std::make_unique<ScreenProvider>()),
      focus_client_(focus_client),
      user_activity_monitor_(std::make_unique<UserActivityMonitor>()),
      next_client_id_(decrement_client_ids ? kInitialClientIdDecrement
                                           : kInitialClientId),
      decrement_client_ids_(decrement_client_ids),
      ime_registrar_(&ime_driver_) {
  DCHECK(focus_client);  // A |focus_client| must be provided.
  // MouseLocationManager is necessary for providing the shared memory with the
  // location of the mouse to clients.
  aura::Env::GetInstance()->CreateMouseLocationManager();

  input_device_server_.RegisterAsObserver();

  // This property should be registered by the PropertyConverter constructor,
  // but that would create a dependency cycle between ui/wm and ui/aura.
  property_converter_.RegisterPrimitiveProperty(
      ::wm::kShadowElevationKey,
      ui::mojom::WindowManager::kShadowElevation_Property,
      aura::PropertyConverter::CreateAcceptAnyValueCallback());
}

WindowService::~WindowService() {
  // WindowTreeFactory owns WindowTrees created by way of WindowTreeFactory.
  // Deleting it should ensure there are no WindowTrees left. Additionally,
  // WindowTree makes use of ScreenProvider, so we need to ensure all the
  // WindowTrees are destroyed before ScreenProvider.
  window_tree_factory_.reset();
  DCHECK(window_trees_.empty());
}

ServerWindow* WindowService::GetServerWindowForWindowCreateIfNecessary(
    aura::Window* window) {
  ServerWindow* server_window = ServerWindow::GetMayBeNull(window);
  if (server_window)
    return server_window;

  const viz::FrameSinkId frame_sink_id =
      ClientWindowId(kWindowServerClientId, next_window_id_++);
  CHECK_NE(0u, next_window_id_);
  const bool is_top_level = false;
  return ServerWindow::Create(window, nullptr, frame_sink_id, is_top_level);
}

std::unique_ptr<WindowTree> WindowService::CreateWindowTree(
    mojom::WindowTreeClient* window_tree_client,
    const std::string& client_name) {
  const ClientSpecificId client_id =
      decrement_client_ids_ ? next_client_id_-- : next_client_id_++;
  CHECK_NE(kInvalidClientId, next_client_id_);
  CHECK_NE(kWindowServerClientId, next_client_id_);
  auto window_tree = std::make_unique<WindowTree>(
      this, client_id, window_tree_client, client_name);
  window_trees_.insert(window_tree.get());
  return window_tree;
}

void WindowService::SetFrameDecorationValues(
    const gfx::Insets& client_area_insets,
    int max_title_bar_button_width) {
  screen_provider_->SetFrameDecorationValues(client_area_insets,
                                             max_title_bar_button_width);
}

// static
bool WindowService::HasRemoteClient(const aura::Window* window) {
  return ServerWindow::GetMayBeNull(window);
}

void WindowService::OnFirstSurfaceActivation(const std::string& client_name) {
  if (surface_activation_callback_)
    std::move(surface_activation_callback_).Run(client_name);
}

void WindowService::OnWillDestroyWindowTree(WindowTree* tree) {
  window_trees_.erase(tree);
}

void WindowService::RequestClose(aura::Window* window) {
  ServerWindow* server_window = ServerWindow::GetMayBeNull(window);
  DCHECK(window && server_window->IsTopLevel());
  server_window->owning_window_tree()->RequestClose(server_window);
}

void WindowService::OnDisplayMetricsChanged(const display::Display& display,
                                            uint32_t changed_metrics) {
  screen_provider_->DisplayMetricsChanged(display, changed_metrics);
}

std::string WindowService::GetIdForDebugging(aura::Window* window) {
  ServerWindow* server_window = ServerWindow::GetMayBeNull(window);
  if (!server_window)
    return std::string();
  return server_window->GetIdForDebugging();
}

void WindowService::OnStart() {
  test_config_ = base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kUseTestConfig);

  window_tree_factory_ = std::make_unique<WindowTreeFactory>(this);

  registry_.AddInterface(base::BindRepeating(
      &WindowService::BindClipboardHostRequest, base::Unretained(this)));
  registry_.AddInterface(base::BindRepeating(
      &WindowService::BindImeRegistrarRequest, base::Unretained(this)));
  registry_.AddInterface(base::BindRepeating(
      &WindowService::BindImeDriverRequest, base::Unretained(this)));
  registry_.AddInterface(base::BindRepeating(
      &WindowService::BindInputDeviceServerRequest, base::Unretained(this)));
  registry_.AddInterface(
      base::BindRepeating(&WindowService::BindRemotingEventInjectorRequest,
                          base::Unretained(this)));
  registry_.AddInterface(base::BindRepeating(
      &WindowService::BindUserActivityMonitorRequest, base::Unretained(this)));

  registry_with_source_info_.AddInterface<mojom::WindowTreeFactory>(
      base::BindRepeating(&WindowService::BindWindowTreeFactoryRequest,
                          base::Unretained(this)));

  // Placeholder to prevent test crashes on unbound requests.
  registry_.AddInterface(base::BindRepeating(&BindEventInjectorRequest));

  // |gpu_interface_provider_| may be null in tests.
  if (gpu_interface_provider_) {
    gpu_interface_provider_->RegisterGpuInterfaces(&registry_);

#if defined(USE_OZONE)
    gpu_interface_provider_->RegisterOzoneGpuInterfaces(&registry_);
#endif
  }

  if (test_config_) {
    registry_.AddInterface<mojom::WindowServerTest>(base::BindRepeating(
        &WindowService::BindWindowServerTestRequest, base::Unretained(this)));
  }
}

void WindowService::OnBindInterface(
    const service_manager::BindSourceInfo& remote_info,
    const std::string& interface_name,
    mojo::ScopedMessagePipeHandle handle) {
  if (!registry_with_source_info_.TryBindInterface(interface_name, &handle,
                                                   remote_info)) {
    registry_.BindInterface(interface_name, std::move(handle));
  }
}

void WindowService::SetSurfaceActivationCallback(
    base::OnceCallback<void(const std::string&)> callback) {
  // Surface activation callbacks are expensive, and allowed only in tests.
  DCHECK(test_config_);
  DCHECK(surface_activation_callback_.is_null() || callback.is_null());
  surface_activation_callback_ = std::move(callback);
}

void WindowService::BindClipboardHostRequest(
    mojom::ClipboardHostRequest request) {
  if (!clipboard_host_)
    clipboard_host_ = std::make_unique<ClipboardHost>();
  clipboard_host_->AddBinding(std::move(request));
}

void WindowService::BindImeRegistrarRequest(
    mojom::IMERegistrarRequest request) {
  ime_registrar_.AddBinding(std::move(request));
}

void WindowService::BindImeDriverRequest(mojom::IMEDriverRequest request) {
  ime_driver_.AddBinding(std::move(request));
}

void WindowService::BindInputDeviceServerRequest(
    mojom::InputDeviceServerRequest request) {
  input_device_server_.AddBinding(std::move(request));
}

void WindowService::BindRemotingEventInjectorRequest(
    mojom::RemotingEventInjectorRequest request) {
  if (!remoting_event_injector_ && delegate_->GetSystemInputInjector()) {
    remoting_event_injector_ = std::make_unique<RemotingEventInjector>(
        delegate_->GetSystemInputInjector());
  }
  if (remoting_event_injector_)
    remoting_event_injector_->AddBinding(std::move(request));
}

void WindowService::BindUserActivityMonitorRequest(
    mojom::UserActivityMonitorRequest request) {
  user_activity_monitor_->AddBinding(std::move(request));
}

void WindowService::BindWindowServerTestRequest(
    mojom::WindowServerTestRequest request) {
  if (!test_config_)
    return;
  mojo::MakeStrongBinding(std::make_unique<WindowServerTestImpl>(this),
                          std::move(request));
}

void WindowService::BindWindowTreeFactoryRequest(
    mojom::WindowTreeFactoryRequest request,
    const service_manager::BindSourceInfo& source_info) {
  window_tree_factory_->AddBinding(std::move(request),
                                   source_info.identity.name());
}

}  // namespace ws2
}  // namespace ui

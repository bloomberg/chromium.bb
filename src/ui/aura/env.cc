// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/env.h"

#include "base/command_line.h"
#include "base/lazy_instance.h"
#include "base/memory/ptr_util.h"
#include "services/ws/public/mojom/window_tree.mojom.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/env_input_state_controller.h"
#include "ui/aura/env_observer.h"
#include "ui/aura/input_state_lookup.h"
#include "ui/aura/local/window_port_local.h"
#include "ui/aura/mouse_location_manager.h"
#include "ui/aura/mus/mus_types.h"
#include "ui/aura/mus/os_exchange_data_provider_mus.h"
#include "ui/aura/mus/system_input_injector_mus.h"
#include "ui/aura/mus/window_port_mus.h"
#include "ui/aura/mus/window_tree_client.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher_observer.h"
#include "ui/aura/window_occlusion_tracker.h"
#include "ui/aura/window_port_for_shutdown.h"
#include "ui/base/ui_base_features.h"
#include "ui/events/event_target_iterator.h"
#include "ui/events/gestures/gesture_recognizer_impl.h"
#include "ui/events/platform/platform_event_source.h"

#if defined(USE_OZONE)
#include "ui/ozone/public/ozone_platform.h"
#endif

namespace aura {

namespace {

// Instance created by all static functions, except
// CreateLocalInstanceForInProcess(). See GetInstance() for details.
Env* g_primary_instance = nullptr;

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// Env, public:

Env::~Env() {
  if (is_os_exchange_data_provider_factory_)
    ui::OSExchangeDataProviderFactory::SetFactory(nullptr);
  if (is_override_input_injector_factory_)
    ui::SetSystemInputInjectorFactory(nullptr);

  for (EnvObserver& observer : observers_)
    observer.OnWillDestroyEnv();

  if (this == g_primary_instance)
    g_primary_instance = nullptr;
}

// static
std::unique_ptr<Env> Env::CreateInstance(Mode mode) {
  DCHECK(!g_primary_instance);
  // No make_unique as constructor is private.
  std::unique_ptr<Env> env(new Env(mode));
  g_primary_instance = env.get();
  env->Init(nullptr);
  return env;
}

// static
std::unique_ptr<Env> Env::CreateLocalInstanceForInProcess() {
  // It is expected this constructor is called *after* an instance has been
  // created of type MUS. The order, and DCHECKs, aren't strictly necessary but
  // help reinforce when this should be used.
  DCHECK(g_primary_instance);
  DCHECK(g_primary_instance->mode() == Mode::MUS);
  // No make_unique as constructor is private.
  std::unique_ptr<Env> env(new Env(Mode::LOCAL));
  env->Init(nullptr);
  return env;
}

#if defined(USE_OZONE)
// static
std::unique_ptr<Env> Env::CreateInstanceToHostViz(
    service_manager::Connector* connector) {
  DCHECK(!g_primary_instance);
  // No make_unique as constructor is private.
  std::unique_ptr<Env> env(new Env(Mode::LOCAL));
  g_primary_instance = env.get();
  env->Init(connector);
  return env;
}
#endif

// static
Env* Env::GetInstance() {
  Env* env = g_primary_instance;
  DCHECK(env) << "Env::CreateInstance must be called before getting the "
                 "instance of Env.";
  return env;
}

// static
bool Env::HasInstance() {
  return !!g_primary_instance;
}

std::unique_ptr<WindowPort> Env::CreateWindowPort(Window* window) {
  if (mode_ == Mode::LOCAL)
    return std::make_unique<WindowPortLocal>(window);

  if (in_mus_shutdown_)
    return std::make_unique<WindowPortForShutdown>();

  DCHECK(window_tree_client_);
  return std::make_unique<WindowPortMus>(window_tree_client_,
                                         WindowMusType::LOCAL);
}

void Env::AddObserver(EnvObserver* observer) {
  observers_.AddObserver(observer);
}

void Env::RemoveObserver(EnvObserver* observer) {
  observers_.RemoveObserver(observer);
}

void Env::AddWindowEventDispatcherObserver(
    WindowEventDispatcherObserver* observer) {
  window_event_dispatcher_observers_.AddObserver(observer);
}

void Env::RemoveWindowEventDispatcherObserver(
    WindowEventDispatcherObserver* observer) {
  window_event_dispatcher_observers_.RemoveObserver(observer);
}

bool Env::IsMouseButtonDown() const {
  return input_state_lookup_.get() ? input_state_lookup_->IsMouseButtonDown() :
      mouse_button_flags_ != 0;
}

const gfx::Point& Env::last_mouse_location() const {
  if (mode_ == Mode::LOCAL || always_use_last_mouse_location_ ||
      !get_last_mouse_location_from_mus_) {
    return last_mouse_location_;
  }

  // Some tests may not install a WindowTreeClient, and we allow multiple
  // WindowTreeClients for the case of multiple connections, and this may be
  // called during shutdown, when there is no WindowTreeClient.
  if (window_tree_client_)
    last_mouse_location_ = window_tree_client_->GetCursorScreenPoint();
  return last_mouse_location_;
}

void Env::SetLastMouseLocation(const gfx::Point& last_mouse_location) {
  last_mouse_location_ = last_mouse_location;
  if (mouse_location_manager_)
    mouse_location_manager_->SetMouseLocation(last_mouse_location);
}

void Env::CreateMouseLocationManager() {
  if (!mouse_location_manager_)
    mouse_location_manager_ = std::make_unique<MouseLocationManager>();
}

mojo::ScopedSharedBufferHandle Env::GetLastMouseLocationMemory() {
  DCHECK(mouse_location_manager_);
  return mouse_location_manager_->GetMouseLocationMemory();
}

void Env::SetWindowTreeClient(WindowTreeClient* window_tree_client) {
  // The WindowTreeClient should only be set once. Test code may need to change
  // the value after the fact, to do that use EnvTestHelper.
  DCHECK(!window_tree_client_);
  window_tree_client_ = window_tree_client;
}

void Env::ScheduleEmbed(
    ws::mojom::WindowTreeClientPtr client,
    base::OnceCallback<void(const base::UnguessableToken&)> callback) {
  DCHECK_EQ(Mode::MUS, mode_);
  DCHECK(window_tree_client_);
  window_tree_client_->ScheduleEmbed(std::move(client), std::move(callback));
}

WindowOcclusionTracker* Env::GetWindowOcclusionTracker() {
  // TODO(https://crbug.com/867150): DCHECK to ensure LOCAL aura after mus
  //     code path is wired up.
  if (!window_occlusion_tracker_) {
    // Use base::WrapUnique + new because of the constructor is private.
    window_occlusion_tracker_ = base::WrapUnique(new WindowOcclusionTracker());
  }

  return window_occlusion_tracker_.get();
}

////////////////////////////////////////////////////////////////////////////////
// Env, private:

// static
bool Env::initial_throttle_input_on_resize_ = true;

Env::Env(Mode mode)
    : mode_(mode),
      env_controller_(std::make_unique<EnvInputStateController>(this)),
      mouse_button_flags_(0),
      is_touch_down_(false),
      get_last_mouse_location_from_mus_(mode_ == Mode::MUS),
      gesture_recognizer_(std::make_unique<ui::GestureRecognizerImpl>()),
      input_state_lookup_(InputStateLookup::Create()),
      context_factory_(nullptr),
      context_factory_private_(nullptr) {}

void Env::Init(service_manager::Connector* connector) {
  if (mode_ == Mode::MUS) {
    EnableMusOSExchangeDataProvider();
    EnableMusOverrideInputInjector();
    return;
  }

#if defined(USE_OZONE)
  // The ozone platform can provide its own event source. So initialize the
  // platform before creating the default event source. If running inside mus
  // let the mus process initialize ozone instead.
  ui::OzonePlatform::InitParams params;
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  // TODO(kylechar): Pass in single process information to Env::CreateInstance()
  // instead of checking flags here.
  params.single_process = command_line->HasSwitch("single-process") ||
                          command_line->HasSwitch("in-process-gpu");
  params.using_mojo = features::IsOzoneDrmMojo();

  if (connector) {
    // Supplying a connector implies this process is hosting Viz.
    params.connector = connector;
    // Hosting viz is currently single-process only.
    params.single_process = true;
    params.using_mojo = true;
  }

  ui::OzonePlatform::InitializeForUI(params);
#endif
  if (!ui::PlatformEventSource::GetInstance())
    event_source_ = ui::PlatformEventSource::CreateDefault();
}

void Env::EnableMusOSExchangeDataProvider() {
  if (!is_os_exchange_data_provider_factory_) {
    ui::OSExchangeDataProviderFactory::SetFactory(this);
    is_os_exchange_data_provider_factory_ = true;
  }
}

void Env::EnableMusOverrideInputInjector() {
  if (!is_override_input_injector_factory_) {
    ui::SetSystemInputInjectorFactory(this);
    is_override_input_injector_factory_ = true;
  }
}

void Env::NotifyWindowInitialized(Window* window) {
  for (EnvObserver& observer : observers_)
    observer.OnWindowInitialized(window);
}

void Env::NotifyHostInitialized(WindowTreeHost* host) {
  for (EnvObserver& observer : observers_)
    observer.OnHostInitialized(host);
}

void Env::NotifyHostActivated(WindowTreeHost* host) {
  for (EnvObserver& observer : observers_)
    observer.OnHostActivated(host);
}

void Env::WindowTreeClientDestroyed(aura::WindowTreeClient* client) {
  DCHECK_EQ(Mode::MUS, mode_);

  if (client != window_tree_client_)
    return;

  in_mus_shutdown_ = true;
  window_tree_client_ = nullptr;
}

////////////////////////////////////////////////////////////////////////////////
// Env, ui::EventTarget implementation:

bool Env::CanAcceptEvent(const ui::Event& event) {
  return true;
}

ui::EventTarget* Env::GetParentTarget() {
  return NULL;
}

std::unique_ptr<ui::EventTargetIterator> Env::GetChildIterator() const {
  return nullptr;
}

ui::EventTargeter* Env::GetEventTargeter() {
  NOTREACHED();
  return NULL;
}

std::unique_ptr<ui::OSExchangeData::Provider> Env::BuildProvider() {
  return std::make_unique<aura::OSExchangeDataProviderMus>();
}

std::unique_ptr<ui::SystemInputInjector> Env::CreateSystemInputInjector() {
  return std::make_unique<SystemInputInjectorMus>(
      window_tree_client_ ? window_tree_client_->connector() : nullptr);
}

}  // namespace aura

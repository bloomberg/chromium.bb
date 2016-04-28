// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/mus/window_manager_connection.h"

#include <utility>

#include "base/lazy_instance.h"
#include "base/threading/thread_local.h"
#include "components/mus/public/cpp/property_type_converters.h"
#include "components/mus/public/cpp/window.h"
#include "components/mus/public/cpp/window_property.h"
#include "components/mus/public/cpp/window_tree_connection.h"
#include "components/mus/public/interfaces/input_event_matcher.mojom.h"
#include "components/mus/public/interfaces/window_tree.mojom.h"
#include "mojo/converters/geometry/geometry_type_converters.h"
#include "services/shell/public/cpp/connection.h"
#include "services/shell/public/cpp/connector.h"
#include "ui/events/devices/device_data_manager.h"
#include "ui/views/mus/native_widget_mus.h"
#include "ui/views/mus/screen_mus.h"
#include "ui/views/pointer_watcher.h"
#include "ui/views/views_delegate.h"

namespace views {
namespace {

using WindowManagerConnectionPtr =
    base::ThreadLocalPointer<views::WindowManagerConnection>;

// Env is thread local so that aura may be used on multiple threads.
base::LazyInstance<WindowManagerConnectionPtr>::Leaky lazy_tls_ptr =
    LAZY_INSTANCE_INITIALIZER;

}  // namespace

// static
void WindowManagerConnection::Create(shell::Connector* connector,
                                     const shell::Identity& identity) {
  DCHECK(!lazy_tls_ptr.Pointer()->Get());
  lazy_tls_ptr.Pointer()->Set(new WindowManagerConnection(connector, identity));
}

// static
WindowManagerConnection* WindowManagerConnection::Get() {
  WindowManagerConnection* connection = lazy_tls_ptr.Pointer()->Get();
  DCHECK(connection);
  return connection;
}

// static
bool WindowManagerConnection::Exists() {
  return !!lazy_tls_ptr.Pointer()->Get();
}

// static
void WindowManagerConnection::Reset() {
  delete Get();
  lazy_tls_ptr.Pointer()->Set(nullptr);
}

mus::Window* WindowManagerConnection::NewWindow(
    const std::map<std::string, std::vector<uint8_t>>& properties) {
  return window_tree_connection_->NewTopLevelWindow(&properties);
}

NativeWidget* WindowManagerConnection::CreateNativeWidgetMus(
    const std::map<std::string, std::vector<uint8_t>>& props,
    const Widget::InitParams& init_params,
    internal::NativeWidgetDelegate* delegate) {
  std::map<std::string, std::vector<uint8_t>> properties = props;
  NativeWidgetMus::ConfigurePropertiesForNewWindow(init_params, &properties);
  properties[mus::mojom::WindowManager::kAppID_Property] =
      mojo::ConvertTo<std::vector<uint8_t>>(identity_.name());
  return new NativeWidgetMus(delegate, connector_, NewWindow(properties),
                             mus::mojom::SurfaceType::DEFAULT);
}

void WindowManagerConnection::AddPointerWatcher(PointerWatcher* watcher) {
  bool had_watcher = HasPointerWatcher();
  pointer_watchers_.AddObserver(watcher);
  if (!had_watcher) {
    // Start a watcher for pointer down.
    // TODO(jamescook): Extend event observers to handle multiple event types.
    mus::mojom::EventMatcherPtr matcher = mus::mojom::EventMatcher::New();
    matcher->type_matcher = mus::mojom::EventTypeMatcher::New();
    matcher->type_matcher->type = mus::mojom::EventType::POINTER_DOWN;
    window_tree_connection_->SetEventObserver(std::move(matcher));
  }
}

void WindowManagerConnection::RemovePointerWatcher(PointerWatcher* watcher) {
  pointer_watchers_.RemoveObserver(watcher);
  if (!HasPointerWatcher()) {
    // Last PointerWatcher removed, stop the event observer.
    window_tree_connection_->SetEventObserver(nullptr);
  }
}

WindowManagerConnection::WindowManagerConnection(
    shell::Connector* connector,
    const shell::Identity& identity)
    : connector_(connector),
      identity_(identity),
      window_tree_connection_(nullptr) {
  window_tree_connection_.reset(
      mus::WindowTreeConnection::Create(this, connector_));

  screen_.reset(new ScreenMus(this));
  screen_->Init(connector);

  // TODO(sad): We should have a DeviceDataManager implementation that talks to
  // a mojo service to learn about the input-devices on the system.
  // http://crbug.com/601981
  ui::DeviceDataManager::CreateInstance();

  ViewsDelegate::GetInstance()->set_native_widget_factory(base::Bind(
      &WindowManagerConnection::CreateNativeWidgetMus,
      base::Unretained(this),
      std::map<std::string, std::vector<uint8_t>>()));
}

WindowManagerConnection::~WindowManagerConnection() {
  // ~WindowTreeConnection calls back to us (we're the WindowTreeDelegate),
  // destroy it while we are still valid.
  window_tree_connection_.reset();

  ui::DeviceDataManager::DeleteInstance();
}

bool WindowManagerConnection::HasPointerWatcher() {
  // Check to see if we really have any observers left. This doesn't use
  // base::ObserverList<>::might_have_observers() because that returns true
  // during iteration over the list even when the last observer is removed.
  base::ObserverList<PointerWatcher>::Iterator iterator(&pointer_watchers_);
  return !!iterator.GetNext();
}

void WindowManagerConnection::OnEmbed(mus::Window* root) {}

void WindowManagerConnection::OnConnectionLost(
    mus::WindowTreeConnection* connection) {}

void WindowManagerConnection::OnEventObserved(const ui::Event& event) {
  if (event.type() == ui::ET_MOUSE_PRESSED) {
    FOR_EACH_OBSERVER(PointerWatcher, pointer_watchers_,
                      OnMousePressed(*event.AsMouseEvent()));
  } else if (event.type() == ui::ET_TOUCH_PRESSED) {
    FOR_EACH_OBSERVER(PointerWatcher, pointer_watchers_,
                      OnTouchPressed(*event.AsTouchEvent()));
  }
}

void WindowManagerConnection::OnWindowManagerFrameValuesChanged() {
  if (window_tree_connection_)
    NativeWidgetMus::NotifyFrameChanged(window_tree_connection_.get());
}

}  // namespace views

// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/test/event_generator_delegate_aura.h"

#include "base/bind.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "services/service_manager/public/cpp/connector.h"
#include "services/ws/public/mojom/constants.mojom.h"
#include "services/ws/public/mojom/event_injector.mojom.h"
#include "ui/aura/client/screen_position_client.h"
#include "ui/aura/env.h"
#include "ui/aura/mus/window_port_mus.h"
#include "ui/aura/test/default_event_generator_delegate.h"
#include "ui/aura/test/env_test_helper.h"
#include "ui/aura/test/mus/window_tree_client_test_api.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/ime/input_method.h"
#include "ui/display/screen.h"
#include "ui/events/event_sink.h"
#include "ui/events/event_source.h"
#include "ui/events/event_target_iterator.h"

namespace aura {
namespace test {
namespace {

// EventTargeterMus serves to send events to the remote window-service by way
// of the RemoteEventInjector interface.
class EventTargeterMus : public ui::EventTarget,
                         public ui::EventTargeter,
                         public ui::EventSource,
                         public ui::EventSink {
 public:
  explicit EventTargeterMus(service_manager::Connector* connector)
      : connector_(connector) {
    DCHECK(connector);
  }
  ~EventTargeterMus() override = default;

  // ui::EventTargeter:
  ui::EventTarget* FindTargetForEvent(ui::EventTarget* root,
                                      ui::Event* event) override {
    return this;
  }
  ui::EventTarget* FindNextBestTarget(ui::EventTarget* previous_target,
                                      ui::Event* event) override {
    return this;
  }

  // ui::EventTarget:
  bool CanAcceptEvent(const ui::Event& event) override { return true; }
  ui::EventTarget* GetParentTarget() override { return nullptr; }
  std::unique_ptr<ui::EventTargetIterator> GetChildIterator() const override {
    return nullptr;
  }
  ui::EventTargeter* GetEventTargeter() override { return this; }

  // ui::EventSource:
  ui::EventSink* GetEventSink() override { return this; }

  // ui::EventSink:
  ui::EventDispatchDetails OnEventFromSource(ui::Event* event) override {
    if (!remote_event_injector_) {
      connector_->BindInterface(ws::mojom::kServiceName,
                                &remote_event_injector_);
    }
    base::RunLoop run_loop;
    // GestureEvent should never be remotely injected (they are generated from
    // TouchEvents).
    DCHECK(!event->IsGestureEvent());
    remote_event_injector_->InjectEvent(
        display::Screen::GetScreen()->GetPrimaryDisplay().id(),
        ui::Event::Clone(*event),
        base::BindOnce(
            [](base::RunLoop* run_loop, bool success) {
              // NOTE: a failure is not necessarily fatal, or result in the test
              // failing.
              if (!success)
                LOG(ERROR) << "Remote event injection failed";
              run_loop->Quit();
            },
            &run_loop));
    run_loop.Run();
    return ui::EventDispatchDetails();
  }

 private:
  service_manager::Connector* connector_;
  ws::mojom::EventInjectorPtr remote_event_injector_;

  DISALLOW_COPY_AND_ASSIGN(EventTargeterMus);
};

// EventGeneratorDelegate implementation for mus.
class EventGeneratorDelegateMus : public EventGeneratorDelegateAura {
 public:
  explicit EventGeneratorDelegateMus(service_manager::Connector* connector)
      : event_targeter_(connector) {}

  ~EventGeneratorDelegateMus() override = default;

  // EventGeneratorDelegateAura:
  ui::EventTarget* GetTargetAt(const gfx::Point& location) override {
    return &event_targeter_;
  }
  ui::EventSource* GetEventSource(ui::EventTarget* target) override {
    return target == &event_targeter_
               ? &event_targeter_
               : EventGeneratorDelegateAura::GetEventSource(target);
  }
  gfx::Point CenterOfTarget(const ui::EventTarget* target) const override {
    if (target != &event_targeter_)
      return EventGeneratorDelegateAura::CenterOfTarget(target);
    display::Screen* const screen = display::Screen::GetScreen();
    return screen->GetPrimaryDisplay().bounds().CenterPoint();
  }
  void ConvertPointFromTarget(const ui::EventTarget* target,
                              gfx::Point* point) const override {
    if (target != &event_targeter_)
      EventGeneratorDelegateAura::ConvertPointFromTarget(target, point);
  }
  void ConvertPointToTarget(const ui::EventTarget* target,
                            gfx::Point* point) const override {
    if (target != &event_targeter_)
      EventGeneratorDelegateAura::ConvertPointToTarget(target, point);
  }
  void ConvertPointFromHost(const ui::EventTarget* hosted_target,
                            gfx::Point* point) const override {
    if (hosted_target != &event_targeter_)
      EventGeneratorDelegateAura::ConvertPointFromHost(hosted_target, point);
  }

 private:
  EventTargeterMus event_targeter_;

  DISALLOW_COPY_AND_ASSIGN(EventGeneratorDelegateMus);
};

const Window* WindowFromTarget(const ui::EventTarget* event_target) {
  return static_cast<const Window*>(event_target);
}
Window* WindowFromTarget(ui::EventTarget* event_target) {
  return static_cast<Window*>(event_target);
}

}  // namespace

// static
std::unique_ptr<ui::test::EventGeneratorDelegate>
EventGeneratorDelegateAura::Create(service_manager::Connector* connector,
                                   ui::test::EventGenerator* owner,
                                   gfx::NativeWindow root_window,
                                   gfx::NativeWindow window) {
  // Tests should not create event generators for a "root window" that's not
  // actually the root window.
  if (root_window)
    DCHECK_EQ(root_window, root_window->GetRootWindow());

  // Do not create EventGeneratorDelegateMus if a root window is supplied.
  // Assume that if a root is supplied the event generator should target the
  // specified window, and there is no need to dispatch remotely.
  if (connector && !root_window)
    return std::make_unique<EventGeneratorDelegateMus>(connector);
  return std::make_unique<DefaultEventGeneratorDelegate>(root_window);
}

EventGeneratorDelegateAura::EventGeneratorDelegateAura() = default;

EventGeneratorDelegateAura::~EventGeneratorDelegateAura() = default;

client::ScreenPositionClient*
EventGeneratorDelegateAura::GetScreenPositionClient(
    const Window* window) const {
  return client::GetScreenPositionClient(window->GetRootWindow());
}

ui::EventSource* EventGeneratorDelegateAura::GetEventSource(
    ui::EventTarget* target) {
  return WindowFromTarget(target)->GetHost()->GetEventSource();
}

gfx::Point EventGeneratorDelegateAura::CenterOfTarget(
    const ui::EventTarget* target) const {
  return CenterOfWindow(WindowFromTarget(target));
}

gfx::Point EventGeneratorDelegateAura::CenterOfWindow(
    gfx::NativeWindow window) const {
  return CenterOfWindow(static_cast<const Window*>(window));
}

void EventGeneratorDelegateAura::ConvertPointFromTarget(
    const ui::EventTarget* event_target,
    gfx::Point* point) const {
  ConvertPointFromWindow(WindowFromTarget(event_target), point);
}

void EventGeneratorDelegateAura::ConvertPointToTarget(
    const ui::EventTarget* event_target,
    gfx::Point* point) const {
  DCHECK(point);
  const Window* target = WindowFromTarget(event_target);
  aura::client::ScreenPositionClient* client = GetScreenPositionClient(target);
  if (client)
    client->ConvertPointFromScreen(target, point);
  else
    aura::Window::ConvertPointToTarget(target->GetRootWindow(), target, point);
}

void EventGeneratorDelegateAura::ConvertPointFromWindow(
    gfx::NativeWindow window,
    gfx::Point* point) const {
  return ConvertPointFromWindow(static_cast<const Window*>(window), point);
}

void EventGeneratorDelegateAura::ConvertPointFromHost(
    const ui::EventTarget* hosted_target,
    gfx::Point* point) const {
  const Window* window = WindowFromTarget(hosted_target);
  window->GetHost()->ConvertPixelsToDIP(point);
}

ui::EventDispatchDetails EventGeneratorDelegateAura::DispatchKeyEventToIME(
    ui::EventTarget* target,
    ui::KeyEvent* event) {
  Window* const window = WindowFromTarget(target);
  return window->GetHost()->GetInputMethod()->DispatchKeyEvent(event);
}

gfx::Point EventGeneratorDelegateAura::CenterOfWindow(
    const Window* window) const {
  gfx::Point center = gfx::Rect(window->bounds().size()).CenterPoint();
  ConvertPointFromWindow(window, &center);
  return center;
}

void EventGeneratorDelegateAura::ConvertPointFromWindow(
    const Window* window,
    gfx::Point* point) const {
  DCHECK(point);
  aura::client::ScreenPositionClient* client = GetScreenPositionClient(window);
  if (client)
    client->ConvertPointToScreen(window, point);
  else
    aura::Window::ConvertPointToTarget(window, window->GetRootWindow(), point);
}

}  // namespace test
}  // namespace aura

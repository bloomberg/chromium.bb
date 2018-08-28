// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/test/event_generator_delegate_aura.h"

#include "base/macros.h"
#include "ui/aura/client/screen_position_client.h"
#include "ui/aura/env.h"
#include "ui/aura/test/env_test_helper.h"
#include "ui/aura/test/mus/window_tree_client_private.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/ime/input_method.h"

namespace aura {
namespace test {
namespace {

class DefaultEventGeneratorDelegate : public EventGeneratorDelegateAura {
 public:
  explicit DefaultEventGeneratorDelegate(gfx::NativeWindow root_window)
      : root_window_(root_window) {}

  ~DefaultEventGeneratorDelegate() override = default;

  // EventGeneratorDelegateAura:
  ui::EventTarget* GetTargetAt(const gfx::Point& location) override {
    return root_window_->GetHost()->window();
  }

  client::ScreenPositionClient* GetScreenPositionClient(
      const aura::Window* window) const override {
    return nullptr;
  }

 private:
  Window* root_window_;

  DISALLOW_COPY_AND_ASSIGN(DefaultEventGeneratorDelegate);
};

const Window* WindowFromTarget(const ui::EventTarget* event_target) {
  return static_cast<const Window*>(event_target);
}

}  // namespace

// static
std::unique_ptr<ui::test::EventGeneratorDelegate>
EventGeneratorDelegateAura::Create(ui::test::EventGenerator* owner,
                                   gfx::NativeWindow root_window,
                                   gfx::NativeWindow window) {
  return std::make_unique<DefaultEventGeneratorDelegate>(root_window);
}

EventGeneratorDelegateAura::EventGeneratorDelegateAura() = default;

EventGeneratorDelegateAura::~EventGeneratorDelegateAura() = default;

ui::EventSource* EventGeneratorDelegateAura::GetEventSource(
    ui::EventTarget* target) {
  return static_cast<Window*>(target)->GetHost()->GetEventSource();
}

gfx::Point EventGeneratorDelegateAura::CenterOfTarget(
    const ui::EventTarget* target) const {
  gfx::Point center =
      gfx::Rect(WindowFromTarget(target)->bounds().size()).CenterPoint();
  ConvertPointFromTarget(target, &center);
  return center;
}

gfx::Point EventGeneratorDelegateAura::CenterOfWindow(
    gfx::NativeWindow window) const {
  return CenterOfTarget(window);
}

void EventGeneratorDelegateAura::ConvertPointFromTarget(
    const ui::EventTarget* event_target,
    gfx::Point* point) const {
  DCHECK(point);
  const Window* target = WindowFromTarget(event_target);
  aura::client::ScreenPositionClient* client = GetScreenPositionClient(target);
  if (client)
    client->ConvertPointToScreen(target, point);
  else
    aura::Window::ConvertPointToTarget(target, target->GetRootWindow(), point);
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

void EventGeneratorDelegateAura::ConvertPointFromHost(
    const ui::EventTarget* hosted_target,
    gfx::Point* point) const {
  const Window* window = WindowFromTarget(hosted_target);
  window->GetHost()->ConvertPixelsToDIP(point);
}

ui::EventDispatchDetails EventGeneratorDelegateAura::DispatchKeyEventToIME(
    ui::EventTarget* target,
    ui::KeyEvent* event) {
  Window* window = static_cast<Window*>(target);
  return window->GetHost()->GetInputMethod()->DispatchKeyEvent(event);
}

void EventGeneratorDelegateAura::DispatchEventToPointerWatchers(
    ui::EventTarget* target,
    const ui::PointerEvent& event) {
  // In non-mus aura PointerWatchers are handled by system-wide EventHandlers,
  // for example ash::Shell and ash::PointerWatcherAdapter.
  if (!Env::GetInstance()->HasWindowTreeClient())
    return;

  // Route the event through WindowTreeClient as in production mus. Does nothing
  // if there are no PointerWatchers installed.
  Window* window = static_cast<Window*>(target);
  WindowTreeClient* window_tree_client = EnvTestHelper().GetWindowTreeClient();
  WindowTreeClientPrivate(window_tree_client)
      .CallOnPointerEventObserved(window, ui::Event::Clone(event));
}

}  // namespace test
}  // namespace aura

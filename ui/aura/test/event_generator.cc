// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/test/event_generator.h"

#include "ui/aura/client/screen_position_client.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/aura/window_tree_host.h"

namespace aura {
namespace test {
namespace {

class DefaultEventGeneratorDelegate : public EventGeneratorDelegateAura {
 public:
  explicit DefaultEventGeneratorDelegate(Window* root_window)
      : root_window_(root_window) {}
  virtual ~DefaultEventGeneratorDelegate() {}

  // EventGeneratorDelegateAura overrides:
  virtual WindowTreeHost* GetHostAt(const gfx::Point& point) const OVERRIDE {
    return root_window_->GetHost();
  }

  virtual client::ScreenPositionClient* GetScreenPositionClient(
      const aura::Window* window) const OVERRIDE {
    return NULL;
  }

 private:
  Window* root_window_;

  DISALLOW_COPY_AND_ASSIGN(DefaultEventGeneratorDelegate);
};

const Window* WindowFromTarget(const ui::EventTarget* event_target) {
  return static_cast<const Window*>(event_target);
}

}  // namespace

EventGeneratorDelegateAura::EventGeneratorDelegateAura() {
}

EventGeneratorDelegateAura::~EventGeneratorDelegateAura() {
}

ui::EventTarget* EventGeneratorDelegateAura::GetTargetAt(
    const gfx::Point& location) {
  return GetHostAt(location)->window();
}

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
  window->GetHost()->ConvertPointFromHost(point);
}

}  // namespace test
}  // namespace aura

namespace ui {
namespace test {

// static
EventGeneratorDelegate* EventGenerator::CreateDefaultPlatformDelegate(
    EventGenerator* owner,
    gfx::NativeWindow root_window,
    gfx::NativeWindow window) {
  return new aura::test::DefaultEventGeneratorDelegate(root_window);
}

}  // namespace test
}  // namespace ui

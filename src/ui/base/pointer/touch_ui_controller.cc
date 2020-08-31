// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/pointer/touch_ui_controller.h"

#include <string>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/message_loop/message_loop_current.h"
#include "base/no_destructor.h"
#include "base/trace_event/trace_event.h"
#include "ui/base/ui_base_switches.h"

#if defined(OS_WIN)
#include "base/win/win_util.h"
#include "base/win/windows_version.h"
#include "ui/gfx/win/singleton_hwnd.h"
#include "ui/gfx/win/singleton_hwnd_observer.h"
#endif

namespace ui {

#if defined(OS_WIN)
namespace {

bool IsTabletMode() {
  return base::win::IsWindows10TabletMode(
      gfx::SingletonHwnd::GetInstance()->hwnd());
}

}  // namespace
#endif  // defined(OS_WIN)

TouchUiController::TouchUiScoperForTesting::TouchUiScoperForTesting(
    bool enabled,
    TouchUiController* controller)
    : controller_(controller),
      old_state_(controller_->SetTouchUiState(
          enabled ? TouchUiState::kEnabled : TouchUiState::kDisabled)) {}

TouchUiController::TouchUiScoperForTesting::~TouchUiScoperForTesting() {
  controller_->SetTouchUiState(old_state_);
}

// static
TouchUiController* TouchUiController::Get() {
  static base::NoDestructor<TouchUiController> instance([] {
    const std::string switch_value =
        base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
            switches::kTopChromeTouchUi);
    if (switch_value == switches::kTopChromeTouchUiDisabled)
      return TouchUiState::kDisabled;
    const bool enabled = switch_value == switches::kTopChromeTouchUiEnabled;
    return enabled ? TouchUiState::kEnabled : TouchUiState::kAuto;
  }());
  return instance.get();
}

TouchUiController::TouchUiController(TouchUiState touch_ui_state)
    : touch_ui_state_(touch_ui_state) {
#if defined(OS_WIN)
  if (base::MessageLoopCurrentForUI::IsSet() &&
      (base::win::GetVersion() >= base::win::Version::WIN10)) {
    singleton_hwnd_observer_ =
        std::make_unique<gfx::SingletonHwndObserver>(base::BindRepeating(
            [](HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam) {
              if (message == WM_SETTINGCHANGE)
                Get()->OnTabletModeToggled(IsTabletMode());
            }));
    tablet_mode_ = IsTabletMode();
  }
#endif
}

TouchUiController::~TouchUiController() = default;

void TouchUiController::OnTabletModeToggled(bool enabled) {
  const bool was_touch_ui = touch_ui();
  tablet_mode_ = enabled;
  if (touch_ui() != was_touch_ui) {
    TRACE_EVENT0("ui", "TouchUiController.NotifyListeners");
    callback_list_.Notify();
  }
}

std::unique_ptr<TouchUiController::Subscription>
TouchUiController::RegisterCallback(const base::RepeatingClosure& closure) {
  return callback_list_.Add(closure);
}

TouchUiController::TouchUiState TouchUiController::SetTouchUiState(
    TouchUiState touch_ui_state) {
  const bool was_touch_ui = touch_ui();
  const TouchUiState old_state = std::exchange(touch_ui_state_, touch_ui_state);
  if (touch_ui() != was_touch_ui) {
    TRACE_EVENT0("ui", "TouchUiController.NotifyListeners");
    callback_list_.Notify();
  }
  return old_state;
}

}  // namespace ui

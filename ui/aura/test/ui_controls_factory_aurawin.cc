// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "ui/aura/test/ui_controls_factory_aura.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/test/ui_controls_aura.h"
#include "ui/base/test/ui_controls_internal_win.h"

namespace aura {
namespace test {

namespace {

using ui_controls::DOWN;
using ui_controls::LEFT;
using ui_controls::MIDDLE;
using ui_controls::MouseButton;
using ui_controls::RIGHT;
using ui_controls::UIControlsAura;
using ui_controls::UP;
using namespace ui_controls::internal;

class UIControlsWin : public UIControlsAura {
 public:
  UIControlsWin() {}

  // UIControlsAura overrides:
  bool SendKeyPress(gfx::NativeWindow native_window,
                    ui::KeyboardCode key,
                    bool control,
                    bool shift,
                    bool alt,
                    bool command) override {
    DCHECK(!command);  // No command key on Aura
    HWND window =
        native_window->GetHost()->GetAcceleratedWidget();
    return SendKeyPressImpl(
        window, key, control, shift, alt, base::Closure());
  }
  bool SendKeyPressNotifyWhenDone(gfx::NativeWindow native_window,
                                  ui::KeyboardCode key,
                                  bool control,
                                  bool shift,
                                  bool alt,
                                  bool command,
                                  const base::Closure& task) override {
    DCHECK(!command);  // No command key on Aura
    HWND window =
        native_window->GetHost()->GetAcceleratedWidget();
    return SendKeyPressImpl(window, key, control, shift, alt, task);
  }
  bool SendMouseMove(long screen_x, long screen_y) override {
    return SendMouseMoveImpl(screen_x, screen_y, base::Closure());
  }
  bool SendMouseMoveNotifyWhenDone(long screen_x,
                                   long screen_y,
                                   const base::Closure& task) override {
    return SendMouseMoveImpl(screen_x, screen_y, task);
  }
  bool SendMouseEvents(MouseButton type, int state) override {
    return SendMouseEventsImpl(type, state, base::Closure());
  }
  bool SendMouseEventsNotifyWhenDone(MouseButton type,
                                     int state,
                                     const base::Closure& task) override {
    return SendMouseEventsImpl(type, state, task);
  }
  bool SendMouseClick(MouseButton type) override {
    return SendMouseEvents(type, UP | DOWN);
  }
  void RunClosureAfterAllPendingUIEvents(
      const base::Closure& closure) override {
    // On windows, posting UI events is synchronous so just post the closure.
    DCHECK(base::MessageLoopForUI::IsCurrent());
    base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE, closure);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(UIControlsWin);
};

}  // namespace

UIControlsAura* CreateUIControlsAura(WindowTreeHost* host) {
  return new UIControlsWin();
}

}  // namespace test
}  // namespace aura

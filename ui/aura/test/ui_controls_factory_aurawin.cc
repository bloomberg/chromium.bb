// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
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
  virtual bool SendKeyPress(gfx::NativeWindow native_window,
                            ui::KeyboardCode key,
                            bool control,
                            bool shift,
                            bool alt,
                            bool command) {
    DCHECK(!command);  // No command key on Aura
    HWND window =
        native_window->GetHost()->GetAcceleratedWidget();
    return SendKeyPressImpl(
        window, key, control, shift, alt, base::Closure());
  }
  virtual bool SendKeyPressNotifyWhenDone(gfx::NativeWindow native_window,
                                          ui::KeyboardCode key,
                                          bool control,
                                          bool shift,
                                          bool alt,
                                          bool command,
                                          const base::Closure& task) {
    DCHECK(!command);  // No command key on Aura
    HWND window =
        native_window->GetHost()->GetAcceleratedWidget();
    return SendKeyPressImpl(window, key, control, shift, alt, task);
  }
  virtual bool SendMouseMove(long screen_x, long screen_y) {
    return SendMouseMoveImpl(screen_x, screen_y, base::Closure());
  }
  virtual bool SendMouseMoveNotifyWhenDone(long screen_x,
                                           long screen_y,
                                           const base::Closure& task) {
    return SendMouseMoveImpl(screen_x, screen_y, task);
  }
  virtual bool SendMouseEvents(MouseButton type, int state) {
    return SendMouseEventsImpl(type, state, base::Closure());
  }
  virtual bool SendMouseEventsNotifyWhenDone(MouseButton type,
                                             int state,
                                             const base::Closure& task) {
    return SendMouseEventsImpl(type, state, task);
  }
  virtual bool SendMouseClick(MouseButton type) {
    return SendMouseEvents(type, UP | DOWN);
  }
  virtual void RunClosureAfterAllPendingUIEvents(const base::Closure& closure) {
    // On windows, posting UI events is synchronous so just post the closure.
    base::MessageLoopForUI::current()->PostTask(FROM_HERE, closure);
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

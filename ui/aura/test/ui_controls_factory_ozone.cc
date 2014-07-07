// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/logging.h"
#include "ui/aura/test/ui_controls_factory_aura.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/test/ui_controls_aura.h"

namespace aura {
namespace test {
namespace {

class UIControlsOzone : public ui_controls::UIControlsAura {
 public:
  UIControlsOzone(WindowTreeHost* host) : host_(host) {}

  virtual bool SendKeyPress(gfx::NativeWindow window,
                            ui::KeyboardCode key,
                            bool control,
                            bool shift,
                            bool alt,
                            bool command) OVERRIDE {
    return SendKeyPressNotifyWhenDone(
        window, key, control, shift, alt, command, base::Closure());
  }
  virtual bool SendKeyPressNotifyWhenDone(
      gfx::NativeWindow window,
      ui::KeyboardCode key,
      bool control,
      bool shift,
      bool alt,
      bool command,
      const base::Closure& closure) OVERRIDE {
    DCHECK(!command);  // No command key on Aura
    NOTIMPLEMENTED();
    RunClosureAfterAllPendingUIEvents(closure);
    return true;
  }

  virtual bool SendMouseMove(long screen_x, long screen_y) OVERRIDE {
    return SendMouseMoveNotifyWhenDone(screen_x, screen_y, base::Closure());
  }
  virtual bool SendMouseMoveNotifyWhenDone(
      long screen_x,
      long screen_y,
      const base::Closure& closure) OVERRIDE {
    NOTIMPLEMENTED();
    RunClosureAfterAllPendingUIEvents(closure);
    return true;
  }
  virtual bool SendMouseEvents(ui_controls::MouseButton type,
                               int state) OVERRIDE {
    return SendMouseEventsNotifyWhenDone(type, state, base::Closure());
  }
  virtual bool SendMouseEventsNotifyWhenDone(
      ui_controls::MouseButton type,
      int state,
      const base::Closure& closure) OVERRIDE {
    NOTIMPLEMENTED();
    RunClosureAfterAllPendingUIEvents(closure);
    return true;
  }
  virtual bool SendMouseClick(ui_controls::MouseButton type) OVERRIDE {
    return SendMouseEvents(type, ui_controls::UP | ui_controls::DOWN);
  }
  virtual void RunClosureAfterAllPendingUIEvents(
      const base::Closure& closure) OVERRIDE {
    if (!closure.is_null())
      closure.Run();
  }

 private:
  WindowTreeHost* host_;

  DISALLOW_COPY_AND_ASSIGN(UIControlsOzone);
};

}  // namespace

ui_controls::UIControlsAura* CreateUIControlsAura(WindowTreeHost* host) {
  return new UIControlsOzone(host);
}

}  // namespace test
}  // namespace aura

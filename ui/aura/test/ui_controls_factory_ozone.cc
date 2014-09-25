// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/logging.h"
#include "ui/aura/client/screen_position_client.h"
#include "ui/aura/env.h"
#include "ui/aura/test/aura_test_utils.h"
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
    int flags = button_down_mask_;

    if (control) {
      flags |= ui::EF_CONTROL_DOWN;
      PostKeyEvent(ui::ET_KEY_PRESSED, ui::VKEY_CONTROL, flags);
    }

    if (shift) {
      flags |= ui::EF_SHIFT_DOWN;
      PostKeyEvent(ui::ET_KEY_PRESSED, ui::VKEY_SHIFT, flags);
    }

    if (alt) {
      flags |= ui::EF_ALT_DOWN;
      PostKeyEvent(ui::ET_KEY_PRESSED, ui::VKEY_MENU, flags);
    }

    if (command) {
      flags |= ui::EF_COMMAND_DOWN;
      PostKeyEvent(ui::ET_KEY_PRESSED, ui::VKEY_LWIN, flags);
    }

    PostKeyEvent(ui::ET_KEY_PRESSED, key, flags);
    PostKeyEvent(ui::ET_KEY_RELEASED, key, flags);

    if (alt) {
      flags &= ~ui::EF_ALT_DOWN;
      PostKeyEvent(ui::ET_KEY_RELEASED, ui::VKEY_MENU, flags);
    }

    if (shift) {
      flags &= ~ui::EF_SHIFT_DOWN;
      PostKeyEvent(ui::ET_KEY_RELEASED, ui::VKEY_SHIFT, flags);
    }

    if (control) {
      flags &= ~ui::EF_CONTROL_DOWN;
      PostKeyEvent(ui::ET_KEY_RELEASED, ui::VKEY_CONTROL, flags);
    }

    if (command) {
      flags &= ~ui::EF_COMMAND_DOWN;
      PostKeyEvent(ui::ET_KEY_RELEASED, ui::VKEY_LWIN, flags);
    }

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
    gfx::Point root_location(screen_x, screen_y);
    aura::client::ScreenPositionClient* screen_position_client =
        aura::client::GetScreenPositionClient(host_->window());
    if (screen_position_client) {
      screen_position_client->ConvertPointFromScreen(host_->window(),
                                                     &root_location);
    }
    gfx::Point root_current_location =
        QueryLatestMousePositionRequestInHost(host_);
    host_->ConvertPointFromHost(&root_current_location);

    if (button_down_mask_)
      PostMouseEvent(ui::ET_MOUSE_DRAGGED, root_location, 0, 0);
    else
      PostMouseEvent(ui::ET_MOUSE_MOVED, root_location, 0, 0);

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
    gfx::Point loc = aura::Env::GetInstance()->last_mouse_location();
    aura::client::ScreenPositionClient* screen_position_client =
        aura::client::GetScreenPositionClient(host_->window());
    if (screen_position_client) {
      screen_position_client->ConvertPointFromScreen(host_->window(), &loc);
    }
    int flag = 0;

    switch (type) {
      case ui_controls::LEFT:
        flag = ui::EF_LEFT_MOUSE_BUTTON;
        break;
      case ui_controls::MIDDLE:
        flag = ui::EF_MIDDLE_MOUSE_BUTTON;
        break;
      case ui_controls::RIGHT:
        flag = ui::EF_RIGHT_MOUSE_BUTTON;
        break;
      default:
        NOTREACHED();
        break;
    }

    if (state & ui_controls::DOWN) {
      button_down_mask_ |= flag;
      PostMouseEvent(ui::ET_MOUSE_PRESSED, loc, button_down_mask_ | flag, flag);
    }
    if (state & ui_controls::UP) {
      button_down_mask_ &= ~flag;
      PostMouseEvent(
          ui::ET_MOUSE_RELEASED, loc, button_down_mask_ | flag, flag);
    }

    RunClosureAfterAllPendingUIEvents(closure);
    return true;
  }
  virtual bool SendMouseClick(ui_controls::MouseButton type) OVERRIDE {
    return SendMouseEvents(type, ui_controls::UP | ui_controls::DOWN);
  }
  virtual void RunClosureAfterAllPendingUIEvents(
      const base::Closure& closure) OVERRIDE {
    if (!closure.is_null())
      base::MessageLoop::current()->PostTask(FROM_HERE, closure);
  }

 private:
  void PostKeyEvent(ui::EventType type, ui::KeyboardCode key_code, int flags) {
    base::MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(&UIControlsOzone::PostKeyEventTask,
                   base::Unretained(this),
                   type,
                   key_code,
                   flags));
  }

  void PostKeyEventTask(ui::EventType type,
                        ui::KeyboardCode key_code,
                        int flags) {
    // Do not rewrite injected events. See crbug.com/136465.
    flags |= ui::EF_FINAL;

    ui::KeyEvent key_event(type, key_code, flags);
    host_->PostNativeEvent(&key_event);
  }

  void PostMouseEvent(ui::EventType type,
                      const gfx::PointF& location,
                      int flags,
                      int changed_button_flags) {
    base::MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(&UIControlsOzone::PostMouseEventTask,
                   base::Unretained(this),
                   type,
                   location,
                   flags,
                   changed_button_flags));
  }

  void PostMouseEventTask(ui::EventType type,
                          const gfx::PointF& location,
                          int flags,
                          int changed_button_flags) {
    ui::MouseEvent mouse_event(
        type, location, location, flags, changed_button_flags);

    // This hack is necessary to set the repeat count for clicks.
    ui::MouseEvent mouse_event2(&mouse_event);

    host_->PostNativeEvent(&mouse_event2);
  }

  WindowTreeHost* host_;

  // Mask of the mouse buttons currently down.
  unsigned button_down_mask_ = 0;

  DISALLOW_COPY_AND_ASSIGN(UIControlsOzone);
};

}  // namespace

ui_controls::UIControlsAura* CreateUIControlsAura(WindowTreeHost* host) {
  return new UIControlsOzone(host);
}

}  // namespace test
}  // namespace aura

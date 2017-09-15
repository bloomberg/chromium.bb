// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/mus/system_input_injector_mus.h"

#include "ui/aura/mus/window_manager_delegate.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/event.h"
#include "ui/events/keycodes/keyboard_code_conversion.h"

namespace aura {

namespace {

int KeyboardCodeToModifier(ui::KeyboardCode key) {
  switch (key) {
    case ui::VKEY_MENU:
    case ui::VKEY_LMENU:
    case ui::VKEY_RMENU:
      return ui::MODIFIER_ALT;
    case ui::VKEY_ALTGR:
      return ui::MODIFIER_ALTGR;
    case ui::VKEY_CAPITAL:
      return ui::MODIFIER_CAPS_LOCK;
    case ui::VKEY_CONTROL:
    case ui::VKEY_LCONTROL:
    case ui::VKEY_RCONTROL:
      return ui::MODIFIER_CONTROL;
    case ui::VKEY_LWIN:
    case ui::VKEY_RWIN:
      return ui::MODIFIER_COMMAND;
    case ui::VKEY_SHIFT:
    case ui::VKEY_LSHIFT:
    case ui::VKEY_RSHIFT:
      return ui::MODIFIER_SHIFT;
    default:
      return ui::MODIFIER_NONE;
  }
}

}  // namespace

SystemInputInjectorMus::SystemInputInjectorMus(WindowManagerClient* client)
    : client_(client) {}

SystemInputInjectorMus::~SystemInputInjectorMus() {}

void SystemInputInjectorMus::MoveCursorTo(const gfx::PointF& location) {
  // TODO(erg): This appears to never be receiving the events from the remote
  // side of the connection. I think this is because it doesn't send mouse
  // events before the first paint.
  NOTIMPLEMENTED();
}

void SystemInputInjectorMus::InjectMouseButton(ui::EventFlags button,
                                               bool down) {
  NOTIMPLEMENTED();
}

void SystemInputInjectorMus::InjectMouseWheel(int delta_x, int delta_y) {
  NOTIMPLEMENTED();
}

void SystemInputInjectorMus::InjectKeyEvent(ui::DomCode dom_code,
                                            bool down,
                                            bool suppress_auto_repeat) {
  // |suppress_auto_repeat| is always true, and can be ignored.
  ui::KeyboardCode key_code = ui::DomCodeToUsLayoutKeyboardCode(dom_code);

  int modifier = KeyboardCodeToModifier(key_code);
  if (modifier)
    UpdateModifier(modifier, down);

  ui::KeyEvent e(down ? ui::ET_KEY_PRESSED : ui::ET_KEY_RELEASED, key_code,
                 dom_code, modifiers_.GetModifierFlags());

  // Even when we're dispatching a key event, we need to have a valid display
  // for event targeting, so grab the display of where the cursor currently is.
  display::Screen* screen = display::Screen::GetScreen();
  display::Display display =
      screen->GetDisplayNearestPoint(screen->GetCursorScreenPoint());
  client_->InjectEvent(e, display.id());
}

void SystemInputInjectorMus::UpdateModifier(unsigned int modifier, bool down) {
  if (modifier == ui::MODIFIER_NONE)
    return;

  if (modifier == ui::MODIFIER_CAPS_LOCK)
    modifiers_.UpdateModifier(ui::MODIFIER_MOD3, down);
  else
    modifiers_.UpdateModifier(modifier, down);
}

}  // namespace aura

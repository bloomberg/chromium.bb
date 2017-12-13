// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/wayland_keyboard.h"

#include <sys/mman.h>
#include <wayland-client.h>

#include "base/files/scoped_file.h"
#include "ui/base/ui_features.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/event.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/keycodes/dom/keycode_converter.h"
#include "ui/events/ozone/layout/keyboard_layout_engine.h"
#include "ui/events/ozone/layout/keyboard_layout_engine_manager.h"
#include "ui/ozone/platform/wayland/wayland_connection.h"
#include "ui/ozone/platform/wayland/wayland_window.h"

#if BUILDFLAG(USE_XKBCOMMON)
#include "ui/ozone/platform/wayland/wayland_xkb_keyboard_layout_engine.h"
#endif

namespace ui {

WaylandKeyboard::WaylandKeyboard(wl_keyboard* keyboard,
                                 const EventDispatchCallback& callback)
    : obj_(keyboard),
      callback_(callback),
      evdev_keyboard_(&modifiers_,
                      KeyboardLayoutEngineManager::GetKeyboardLayoutEngine(),
                      callback_) {
  static const wl_keyboard_listener listener = {
      &WaylandKeyboard::Keymap,    &WaylandKeyboard::Enter,
      &WaylandKeyboard::Leave,     &WaylandKeyboard::Key,
      &WaylandKeyboard::Modifiers, &WaylandKeyboard::RepeatInfo,
  };

  wl_keyboard_add_listener(obj_.get(), &listener, this);

  // TODO(tonikitoo): Default auto-repeat to ON here?
}

WaylandKeyboard::~WaylandKeyboard() {}

void WaylandKeyboard::Keymap(void* data,
                             wl_keyboard* obj,
                             uint32_t format,
                             int32_t raw_fd,
                             uint32_t size) {
  base::ScopedFD fd(raw_fd);
  if (!data || format != WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1)
    return;

  char* keymap_str = reinterpret_cast<char*>(
      mmap(nullptr, size, PROT_READ, MAP_SHARED, fd.get(), 0));
  if (keymap_str == MAP_FAILED)
    return;

  bool success =
      KeyboardLayoutEngineManager::GetKeyboardLayoutEngine()
          ->SetCurrentLayoutFromBuffer(keymap_str, strnlen(keymap_str, size));
  DCHECK(success) << "Failed to set the XKB keyboard mapping.";
  munmap(keymap_str, size);
}

void WaylandKeyboard::Enter(void* data,
                            wl_keyboard* obj,
                            uint32_t serial,
                            wl_surface* surface,
                            wl_array* keys) {
  WaylandWindow::FromSurface(surface)->set_keyboard_focus(true);
}

void WaylandKeyboard::Leave(void* data,
                            wl_keyboard* obj,
                            uint32_t serial,
                            wl_surface* surface) {
  WaylandWindow::FromSurface(surface)->set_keyboard_focus(false);

  WaylandKeyboard* keyboard = static_cast<WaylandKeyboard*>(data);

  // Reset all modifiers once focus is lost. Otherwise, the modifiers may be
  // left with old flags, which are no longer valid.
  keyboard->modifiers_.ResetKeyboardModifiers();
}

void WaylandKeyboard::Key(void* data,
                          wl_keyboard* obj,
                          uint32_t serial,
                          uint32_t time,
                          uint32_t key,
                          uint32_t state) {
  WaylandKeyboard* keyboard = static_cast<WaylandKeyboard*>(data);
  keyboard->connection_->set_serial(serial);

  keyboard->evdev_keyboard_.OnKeyChange(
      key, state == WL_KEYBOARD_KEY_STATE_PRESSED, false, EventTimeForNow(),
      keyboard->obj_.id());
}

void WaylandKeyboard::Modifiers(void* data,
                                wl_keyboard* obj,
                                uint32_t serial,
                                uint32_t mods_depressed,
                                uint32_t mods_latched,
                                uint32_t mods_locked,
                                uint32_t group) {
  // KeyboardEvDev handles modifiers.
}

void WaylandKeyboard::RepeatInfo(void* data,
                                 wl_keyboard* obj,
                                 int32_t rate,
                                 int32_t delay) {
  WaylandKeyboard* keyboard = static_cast<WaylandKeyboard*>(data);
  keyboard->evdev_keyboard_.SetAutoRepeatRate(
      base::TimeDelta::FromMilliseconds(delay),
      base::TimeDelta::FromMilliseconds(rate));

  // Keyboard rate less than 0 means, wayland wants to disable autorepeat.
  keyboard->evdev_keyboard_.SetAutoRepeatEnabled(rate > 0 ? true : false);
}

}  // namespace ui

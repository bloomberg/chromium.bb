// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/wayland_keyboard.h"

#include <sys/mman.h>
#include <wayland-client.h>

#include "base/files/scoped_file.h"
#include "ui/base/ui_features.h"
#include "ui/events/event.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/keycodes/dom/keycode_converter.h"
#include "ui/events/ozone/layout/keyboard_layout_engine.h"
#include "ui/events/ozone/layout/keyboard_layout_engine_manager.h"
#include "ui/ozone/platform/wayland/wayland_window.h"

#if BUILDFLAG(USE_XKBCOMMON)
#include "ui/ozone/platform/wayland/wayland_xkb_keyboard_layout_engine.h"
#endif

namespace ui {

namespace {

const int kXkbKeycodeOffset = 8;

}  // namespace

WaylandKeyboard::WaylandKeyboard(wl_keyboard* keyboard,
                                 const EventDispatchCallback& callback)
    : obj_(keyboard), callback_(callback) {
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
}

void WaylandKeyboard::Key(void* data,
                          wl_keyboard* obj,
                          uint32_t serial,
                          uint32_t time,
                          uint32_t key,
                          uint32_t state) {
  WaylandKeyboard* keyboard = static_cast<WaylandKeyboard*>(data);

  DomCode dom_code =
      KeycodeConverter::NativeKeycodeToDomCode(key + kXkbKeycodeOffset);
  if (dom_code == ui::DomCode::NONE)
    return;

  uint8_t flags = keyboard->modifiers_;
  DomKey dom_key;
  KeyboardCode key_code;
  if (!KeyboardLayoutEngineManager::GetKeyboardLayoutEngine()->Lookup(
          dom_code, flags, &dom_key, &key_code))
    return;

  // TODO(tonikitoo): handle repeat here.
  bool down = state == WL_KEYBOARD_KEY_STATE_PRESSED;
  ui::KeyEvent event(
      down ? ET_KEY_PRESSED : ET_KEY_RELEASED, key_code, dom_code,
      keyboard->modifiers_, dom_key,
      base::TimeTicks() + base::TimeDelta::FromMilliseconds(time));
  event.set_source_device_id(keyboard->obj_.id());
  keyboard->callback_.Run(&event);
}

void WaylandKeyboard::Modifiers(void* data,
                                wl_keyboard* obj,
                                uint32_t serial,
                                uint32_t mods_depressed,
                                uint32_t mods_latched,
                                uint32_t mods_locked,
                                uint32_t group) {
#if BUILDFLAG(USE_XKBCOMMON)
  WaylandKeyboard* keyboard = static_cast<WaylandKeyboard*>(data);
  auto* engine = static_cast<WaylandXkbKeyboardLayoutEngine*>(
      KeyboardLayoutEngineManager::GetKeyboardLayoutEngine());

  keyboard->modifiers_ =
      engine->UpdateModifiers(mods_depressed, mods_latched, mods_locked, group);

#endif
}

void WaylandKeyboard::RepeatInfo(void* data,
                                 wl_keyboard* obj,
                                 int32_t rate,
                                 int32_t delay) {
  // TODO(tonikitoo): Implement proper repeat handling.
  NOTIMPLEMENTED();
}

}  // namespace ui

// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_MOCK_WAYLAND_XKB_KEYBOARD_LAYOUT_ENGINE_H_
#define UI_OZONE_PLATFORM_WAYLAND_MOCK_WAYLAND_XKB_KEYBOARD_LAYOUT_ENGINE_H_

#include "ui/base/ui_features.h"

#if !BUILDFLAG(USE_XKBCOMMON)
#error "This file should only be included if USE_XKBCOMMON is enabled."
#endif

#include "ui/events/ozone/layout/xkb/xkb_evdev_codes.h"
#include "ui/ozone/platform/wayland/wayland_xkb_keyboard_layout_engine.h"

namespace ui {

class MockWaylandXkbKeyboardLayoutEngine
    : public ui::WaylandXkbKeyboardLayoutEngine {
 public:
  MockWaylandXkbKeyboardLayoutEngine(const ui::XkbKeyCodeConverter& converter)
      : WaylandXkbKeyboardLayoutEngine(converter) {}

  void UpdateModifiers(uint32_t depressed_mods,
                       uint32_t latched_mods,
                       uint32_t locked_mods,
                       uint32_t group) override {}
  void SetEventModifiers(ui::EventModifiers* event_modifiers) override {}

 private:
  bool Lookup(ui::DomCode dom_code,
              int event_flags,
              ui::DomKey* dom_key,
              ui::KeyboardCode* key_code) const override;

  DISALLOW_COPY_AND_ASSIGN(MockWaylandXkbKeyboardLayoutEngine);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_MOCK_WAYLAND_XKB_KEYBOARD_LAYOUT_ENGINE_H_

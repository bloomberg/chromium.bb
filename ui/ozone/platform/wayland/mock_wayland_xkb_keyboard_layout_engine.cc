// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/mock_wayland_xkb_keyboard_layout_engine.h"
#include "ui/events/keycodes/keyboard_code_conversion.h"

#if !BUILDFLAG(USE_XKBCOMMON)
#error "This file should only be compiled if USE_XKBCOMMON is enabled."
#endif

namespace ui {

bool MockWaylandXkbKeyboardLayoutEngine::Lookup(
    ui::DomCode dom_code,
    int event_flags,
    ui::DomKey* dom_key,
    ui::KeyboardCode* key_code) const {
  return DomCodeToUsLayoutDomKey(dom_code, event_flags, dom_key, key_code);
}

}  // namespace ui

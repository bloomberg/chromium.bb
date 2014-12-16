// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_OZONE_LAYOUT_XKB_XKB_KEYBOARD_CODE_CONVERSION_H_
#define UI_EVENTS_OZONE_LAYOUT_XKB_XKB_KEYBOARD_CODE_CONVERSION_H_

// TODO(kpschoedel): move this file out of Ozone so that it can be used to
// determine DomKey for desktop X11, OR switch desktop X11 to use the Ozone
// keyboard layout interface.

#include <xkbcommon/xkbcommon.h>

#include "base/strings/string16.h"
#include "ui/events/keycodes/keyboard_codes.h"

namespace ui {

enum class DomKey;

// Returns the DomKey associated with a non-character xkb_keysym_t.
// Returns DomKey::NONE for unrecognized keysyms, which includes
// all printable characters.
DomKey NonPrintableXkbKeySymToDomKey(xkb_keysym_t keysym);

// Returns the dead key combining character associated with an xkb_keysym_t,
// or 0 if the keysym is not recognized.
base::char16 DeadXkbKeySymToCombiningCharacter(xkb_keysym_t keysym);

}  // namespace ui

#endif  // UI_EVENTS_OZONE_LAYOUT_XKB_XKB_KEYBOARD_CODE_CONVERSION_H_

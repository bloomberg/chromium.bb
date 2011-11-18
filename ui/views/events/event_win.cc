// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/events/event.h"

#include "base/logging.h"
#include "ui/base/keycodes/keyboard_code_conversion.h"

namespace views {

////////////////////////////////////////////////////////////////////////////////
// KeyEvent, public:

uint16 KeyEvent::GetCharacter() const {
  if (character_)
    return character_;
  return (native_event().message == WM_CHAR) ? key_code_ :
      ui::GetCharacterFromKeyCode(key_code_, flags());
}

uint16 KeyEvent::GetUnmodifiedCharacter() const {
  if (unmodified_character_)
    return unmodified_character_;
  // Looks like there is no way to get unmodified character on Windows.
  return (native_event().message == WM_CHAR) ? key_code_ :
      ui::GetCharacterFromKeyCode(key_code_, flags() & ui::EF_SHIFT_DOWN);
}

}  // namespace views

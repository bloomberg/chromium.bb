// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/content_accelerators/accelerator_util.h"

#include "build/build_config.h"
#include "third_party/blink/public/common/input/web_input_event.h"
#include "ui/base/ui_base_features.h"
#include "ui/events/blink/blink_event_util.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"

namespace ui {

ui::Accelerator GetAcceleratorFromNativeWebKeyboardEvent(
    const content::NativeWebKeyboardEvent& event) {
#if defined(OS_CHROMEOS)
  if (::features::IsNewShortcutMappingEnabled()) {
    // TODO: This must be the same as below and it's simpler.
    // Cleanup if this change sticks.
    auto* os_event = static_cast<ui::KeyEvent*>(event.os_event);
    return ui::Accelerator(*os_event);
  }
#endif
  Accelerator::KeyState key_state =
      event.GetType() == blink::WebInputEvent::Type::kKeyUp
          ? Accelerator::KeyState::RELEASED
          : Accelerator::KeyState::PRESSED;
  ui::KeyboardCode keyboard_code =
      static_cast<ui::KeyboardCode>(event.windows_key_code);
  int modifiers = WebEventModifiersToEventFlags(event.GetModifiers());
  return ui::Accelerator(keyboard_code, modifiers, key_state,
                         event.TimeStamp());
}

}  // namespace ui

// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/wm/core/accelerator_filter.h"

#include "ui/base/accelerators/accelerator.h"
#include "ui/events/event.h"
#include "ui/wm/core/accelerator_delegate.h"

namespace wm {
namespace {

const int kModifierFlagMask =
    (ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN);

// Returns true if |key_code| is a key usually handled directly by the shell.
bool IsSystemKey(ui::KeyboardCode key_code) {
#if defined(OS_CHROMEOS)
  switch (key_code) {
    case ui::VKEY_MEDIA_LAUNCH_APP2:  // Fullscreen button.
    case ui::VKEY_MEDIA_LAUNCH_APP1:  // Overview button.
    case ui::VKEY_BRIGHTNESS_DOWN:
    case ui::VKEY_BRIGHTNESS_UP:
    case ui::VKEY_KBD_BRIGHTNESS_DOWN:
    case ui::VKEY_KBD_BRIGHTNESS_UP:
    case ui::VKEY_VOLUME_MUTE:
    case ui::VKEY_VOLUME_DOWN:
    case ui::VKEY_VOLUME_UP:
      return true;
    default:
      return false;
  }
#endif  // defined(OS_CHROMEOS)
  return false;
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// AcceleratorFilter, public:

AcceleratorFilter::AcceleratorFilter(scoped_ptr<AcceleratorDelegate> delegate)
    : delegate_(delegate.Pass()) {
}

AcceleratorFilter::~AcceleratorFilter() {
}

////////////////////////////////////////////////////////////////////////////////
// AcceleratorFilter, EventFilter implementation:

void AcceleratorFilter::OnKeyEvent(ui::KeyEvent* event) {
  const ui::EventType type = event->type();
  if (type != ui::ET_KEY_PRESSED && type != ui::ET_KEY_RELEASED)
    return;
  if (event->is_char())
    return;

  ui::Accelerator accelerator(event->key_code(),
                              event->flags() & kModifierFlagMask);
  accelerator.set_type(event->type());

  delegate_->PreProcessAccelerator(accelerator);

  // Handle special hardware keys like brightness and volume. However, some
  // windows can override this behavior (e.g. Chrome v1 apps by default and
  // Chrome v2 apps with permission) by setting a window property.
  if (IsSystemKey(event->key_code()) &&
      !delegate_->CanConsumeSystemKeys(*event)) {
    delegate_->ProcessAccelerator(accelerator);
    // These keys are always consumed regardless of whether they trigger an
    // accelerator to prevent windows from seeing unexpected key up events.
    event->StopPropagation();
    return;
  }

  if (!delegate_->ShouldProcessAcceleratorNow(*event, accelerator))
    return;

  if (delegate_->ProcessAccelerator(accelerator))
    event->StopPropagation();
}

}  // namespace wm

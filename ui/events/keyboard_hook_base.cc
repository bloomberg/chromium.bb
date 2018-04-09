// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/keyboard_hook_base.h"

#include <utility>

#include "base/macros.h"
#include "base/stl_util.h"
#include "ui/events/event.h"

namespace ui {

KeyboardHookBase::KeyboardHookBase(
    base::Optional<base::flat_set<int>> native_key_codes,
    KeyEventCallback callback)
    : key_event_callback_(std::move(callback)),
      key_codes_(std::move(native_key_codes)) {
  DCHECK(key_event_callback_);
}

KeyboardHookBase::~KeyboardHookBase() = default;

bool KeyboardHookBase::IsKeyLocked(int native_key_code) {
  return ShouldCaptureKeyEvent(native_key_code);
}

bool KeyboardHookBase::ShouldCaptureKeyEvent(int key_code) const {
  return !key_codes_ || base::ContainsKey(key_codes_.value(), key_code);
}

void KeyboardHookBase::ForwardCapturedKeyEvent(
    std::unique_ptr<KeyEvent> event) {
  key_event_callback_.Run(event.get());
}

}  // namespace ui

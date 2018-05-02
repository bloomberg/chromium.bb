// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/keyboard_hook.h"

#include <memory>

#include "base/callback.h"
#include "base/optional.h"

namespace ui {

// static
std::unique_ptr<KeyboardHook> KeyboardHook::Create(
    base::Optional<base::flat_set<int>> native_key_codes,
    KeyboardHook::KeyEventCallback callback) {
  return nullptr;
}

}  // namespace ui

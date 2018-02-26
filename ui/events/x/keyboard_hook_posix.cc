// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/keyboard_hook_base.h"

#include "base/callback.h"
#include "base/macros.h"
#include "base/optional.h"
#include "ui/events/event.h"

namespace ui {

namespace {

// A default implementation for POSIX platforms.
class KeyboardHookPosix : public KeyboardHookBase {
 public:
  KeyboardHookPosix(base::Optional<base::flat_set<int>> native_key_codes,
                    KeyEventCallback callback);
  ~KeyboardHookPosix() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(KeyboardHookPosix);
};

KeyboardHookPosix::KeyboardHookPosix(
    base::Optional<base::flat_set<int>> key_codes,
    KeyEventCallback callback)
    : KeyboardHookBase(std::move(key_codes), std::move(callback)) {}

KeyboardHookPosix::~KeyboardHookPosix() = default;

}  // namespace

// static
std::unique_ptr<KeyboardHook> KeyboardHook::Create(
    base::Optional<base::flat_set<int>> native_key_codes,
    KeyboardHook::KeyEventCallback callback) {
  return nullptr;
}

}  // namespace ui

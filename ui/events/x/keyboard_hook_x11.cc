// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/callback.h"
#include "base/containers/flat_set.h"
#include "base/macros.h"
#include "base/optional.h"
#include "ui/events/keyboard_hook_base.h"
#include "ui/events/keycodes/dom/dom_code.h"

namespace ui {

namespace {

// A default implementation for the X11 platform.
class KeyboardHookX11 : public KeyboardHookBase {
 public:
  KeyboardHookX11(base::Optional<base::flat_set<DomCode>> dom_codes,
                  KeyEventCallback callback);
  ~KeyboardHookX11() override;

  bool Register();

 private:
  DISALLOW_COPY_AND_ASSIGN(KeyboardHookX11);
};

KeyboardHookX11::KeyboardHookX11(
    base::Optional<base::flat_set<DomCode>> dom_codes,
    KeyEventCallback callback)
    : KeyboardHookBase(std::move(dom_codes), std::move(callback)) {}

KeyboardHookX11::~KeyboardHookX11() = default;

bool KeyboardHookX11::Register() {
  // TODO(680809): Implement system-level keyboard lock feature for X11.
  // Return true to enable browser-level keyboard lock for the X11 platform.
  return true;
}

}  // namespace

// static
std::unique_ptr<KeyboardHook> KeyboardHook::Create(
    base::Optional<base::flat_set<DomCode>> dom_codes,
    KeyboardHook::KeyEventCallback callback) {
  std::unique_ptr<KeyboardHookX11> keyboard_hook =
      std::make_unique<KeyboardHookX11>(std::move(dom_codes),
                                        std::move(callback));

  if (!keyboard_hook->Register())
    return nullptr;

  return keyboard_hook;
}

}  // namespace ui

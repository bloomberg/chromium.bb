// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_KEYBOARD_HOOK_H_
#define UI_EVENTS_KEYBOARD_HOOK_H_

#include <memory>

#include "base/callback.h"
#include "base/containers/flat_set.h"
#include "base/optional.h"
#include "ui/events/events_export.h"

namespace ui {

class KeyEvent;

// Intercepts keyboard events typically handled by the OS or browser.
// Destroying the instance will unregister and clean up the keyboard hook.
class EVENTS_EXPORT KeyboardHook {
 public:
  using KeyEventCallback = base::RepeatingCallback<void(KeyEvent* event)>;

  virtual ~KeyboardHook() = default;

  // Creates a platform specific implementation.
  // |native_key_codes| is the set of key codes which will be intercepted, if
  // it is empty, this class will attempt to intercept all keys.
  // |callback| is called for each key which is intercepted.
  // Returns a valid instance if the hook was created and successfully
  // registered otherwise nullptr.
  // TODO(joedow): Update this interface to use DomCodes.
  static std::unique_ptr<KeyboardHook> Create(
      base::Optional<base::flat_set<int>> native_key_codes,
      KeyEventCallback callback);

  // True if |native_key_code| is reserved for an active KeyboardLock request.
  virtual bool IsKeyLocked(int native_key_code) = 0;
};

}  // namespace ui

#endif  // UI_EVENTS_KEYBOARD_HOOK_H_

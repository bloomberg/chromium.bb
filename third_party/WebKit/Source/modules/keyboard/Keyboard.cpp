// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/keyboard/Keyboard.h"

#include "core/dom/ExecutionContext.h"
#include "modules/keyboard/KeyboardLock.h"
#include "platform/bindings/ScriptState.h"

namespace blink {

Keyboard::Keyboard(ExecutionContext* context)
    : keyboard_lock_(new KeyboardLock(context)) {}

Keyboard::~Keyboard() = default;

ScriptPromise Keyboard::lock(ScriptState* state,
                             const Vector<String>& keycodes) {
  return keyboard_lock_->lock(state, keycodes);
}

void Keyboard::unlock() {
  keyboard_lock_->unlock();
}

void Keyboard::Trace(blink::Visitor* visitor) {
  visitor->Trace(keyboard_lock_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink

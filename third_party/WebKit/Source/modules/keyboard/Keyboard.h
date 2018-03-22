// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef Keyboard_h
#define Keyboard_h

#include "bindings/core/v8/ScriptPromise.h"
#include "platform/bindings/ScriptWrappable.h"

namespace blink {

class ExecutionContext;
class KeyboardLock;
class ScriptState;

class Keyboard final : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();
  WTF_MAKE_NONCOPYABLE(Keyboard);

 public:
  explicit Keyboard(ExecutionContext*);
  ~Keyboard() override;

  // KeyboardLock API: https://w3c.github.io/keyboard-lock/
  ScriptPromise lock(ScriptState*, const Vector<String>&);
  void unlock();

  // ScriptWrappable override.
  void Trace(blink::Visitor*) override;

 private:
  Member<KeyboardLock> keyboard_lock_;
};

}  // namespace blink

#endif  // Keyboard_h

// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef KeyboardLock_h
#define KeyboardLock_h

#include "bindings/core/v8/ScriptPromise.h"
#include "core/CoreExport.h"
#include "core/dom/ContextLifecycleObserver.h"
#include "platform/heap/Member.h"
#include "public/platform/modules/keyboard_lock/keyboard_lock.mojom-blink.h"

namespace blink {

class ScriptPromiseResolver;

class KeyboardLock final : public GarbageCollectedFinalized<KeyboardLock>,
                           public ContextLifecycleObserver {
  USING_GARBAGE_COLLECTED_MIXIN(KeyboardLock);
  WTF_MAKE_NONCOPYABLE(KeyboardLock);

 public:
  explicit KeyboardLock(ExecutionContext*);
  ~KeyboardLock();

  ScriptPromise lock(ScriptState*, const Vector<String>&);
  void unlock();

  // ContextLifecycleObserver override.
  void Trace(blink::Visitor*) override;

 private:
  // Returns true if |service_| is initialized and ready to be called.
  bool EnsureServiceConnected();

  void LockRequestFinished(mojom::KeyboardLockRequestResult);

  mojom::blink::KeyboardLockServicePtr service_;
  Member<ScriptPromiseResolver> request_keylock_resolver_;
};

}  // namespace blink

#endif  // KeyboardLock_h

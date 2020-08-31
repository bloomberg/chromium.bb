// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_KEYBOARD_KEYBOARD_LOCK_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_KEYBOARD_KEYBOARD_LOCK_H_

#include "base/macros.h"
#include "third_party/blink/public/mojom/keyboard_lock/keyboard_lock.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/execution_context/execution_context_lifecycle_observer.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_remote.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_wrapper_mode.h"

namespace blink {

class ExceptionState;
class ScriptPromiseResolver;

class KeyboardLock final : public GarbageCollected<KeyboardLock>,
                           public ExecutionContextClient {
  USING_GARBAGE_COLLECTED_MIXIN(KeyboardLock);

 public:
  explicit KeyboardLock(ExecutionContext*);
  ~KeyboardLock();

  ScriptPromise lock(ScriptState*, const Vector<String>&, ExceptionState&);
  void unlock(ScriptState*);

  void Trace(Visitor*) override;

 private:
  // Returns true if the local frame is attached to the renderer.
  bool IsLocalFrameAttached();

  // Returns true if |service_| is initialized and ready to be called.
  bool EnsureServiceConnected();

  // Returns true if the current frame is a top-level browsing context.
  bool CalledFromSupportedContext(ExecutionContext*);

  void LockRequestFinished(ScriptPromiseResolver*,
                           mojom::KeyboardLockRequestResult);

  HeapMojoRemote<mojom::blink::KeyboardLockService,
                 HeapMojoWrapperMode::kWithoutContextObserver>
      service_;
  Member<ScriptPromiseResolver> request_keylock_resolver_;

  DISALLOW_COPY_AND_ASSIGN(KeyboardLock);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_KEYBOARD_KEYBOARD_LOCK_H_

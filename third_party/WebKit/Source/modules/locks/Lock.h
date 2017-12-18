// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef Lock_h
#define Lock_h

#include "core/dom/PausableObject.h"
#include "platform/bindings/ScriptWrappable.h"
#include "platform/heap/Handle.h"
#include "platform/wtf/Vector.h"
#include "platform/wtf/text/WTFString.h"
#include "public/platform/modules/locks/lock_manager.mojom-blink.h"

namespace blink {

class ScriptPromise;
class ScriptPromiseResolver;
class ScriptState;

class Lock final : public ScriptWrappable, public PausableObject {
  DEFINE_WRAPPERTYPEINFO();
  USING_GARBAGE_COLLECTED_MIXIN(Lock);

 public:
  static Lock* Create(ScriptState*,
                      const String& name,
                      mojom::blink::LockManager::LockMode,
                      mojom::blink::LockHandlePtr);

  ~Lock() override;

  virtual void Trace(blink::Visitor*);
  EAGERLY_FINALIZE();

  // Lock.idl
  String name() const { return name_; }
  String mode() const;

  // PausableObject
  void ContextDestroyed(ExecutionContext*) override;

  // The lock is held until the passed promise resolves. When it is released,
  // the passed resolver is invoked with the promise's result.
  void HoldUntil(ScriptPromise, ScriptPromiseResolver*);

  static mojom::blink::LockManager::LockMode StringToMode(const String&);
  static String ModeToString(mojom::blink::LockManager::LockMode);

 private:
  class ThenFunction;

  Lock(ScriptState*,
       const String& name,
       mojom::blink::LockManager::LockMode,
       mojom::blink::LockHandlePtr);

  void ReleaseIfHeld();

  Member<ScriptPromiseResolver> resolver_;

  const String name_;
  const mojom::blink::LockManager::LockMode mode_;
  mojom::blink::LockHandlePtr handle_;
};

}  // namespace blink

#endif  // Lock_h

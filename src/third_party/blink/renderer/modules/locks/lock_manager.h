// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_LOCKS_LOCK_MANAGER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_LOCKS_LOCK_MANAGER_H_

#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/mojom/feature_observer/feature_observer.mojom-blink.h"
#include "third_party/blink/public/mojom/locks/lock_manager.mojom-blink-forward.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_lock_options.h"
#include "third_party/blink/renderer/modules/locks/lock.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_remote.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_wrapper_mode.h"
#include "third_party/blink/renderer/platform/supplementable.h"

namespace blink {

class NavigatorBase;
class ScriptPromise;
class ScriptState;
class V8LockGrantedCallback;

class LockManager final : public ScriptWrappable,
                          public Supplement<NavigatorBase>,
                          public ExecutionContextLifecycleObserver {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static const char kSupplementName[];

  // Web-exposed as navigator.locks
  static LockManager* locks(NavigatorBase&);

  explicit LockManager(NavigatorBase&);

  LockManager(const LockManager&) = delete;
  LockManager& operator=(const LockManager&) = delete;

  ScriptPromise request(ScriptState*,
                        const String& name,
                        V8LockGrantedCallback*,
                        ExceptionState&);
  ScriptPromise request(ScriptState*,
                        const String& name,
                        const LockOptions*,
                        V8LockGrantedCallback*,
                        ExceptionState&);

  ScriptPromise query(ScriptState*, ExceptionState&);

  void Trace(Visitor*) const override;

  // Terminate all outstanding requests when the context is destroyed, since
  // this can unblock requests by other contexts.
  void ContextDestroyed() override;

  // Called by a lock when it is released. The lock is dropped from the
  // |held_locks_| list. Held locks are tracked until explicitly released (or
  // context is destroyed) to handle the case where both the lock and the
  // promise holding it open have no script references and are potentially
  // collectable. In that case, the lock should be held until the context
  // is destroyed. See https://crbug.com/798500 for an example.
  void OnLockReleased(Lock*);

 private:
  class LockRequestImpl;

  // Track pending requests so that they can be cancelled if the context is
  // terminated.
  void AddPendingRequest(LockRequestImpl*);
  void RemovePendingRequest(LockRequestImpl*);
  bool IsPendingRequest(LockRequestImpl*);

  // Query the ContentSettingsClient to ensure access is allowed from
  // this context. The first call invokes a synchronous IPC call, but
  // the result is cached for subsequent accesses.
  bool AllowLocks(ScriptState* script_state);

  HeapHashSet<Member<LockRequestImpl>> pending_requests_;
  HeapHashSet<Member<Lock>> held_locks_;

  HeapMojoRemote<mojom::blink::LockManager> service_;
  HeapMojoRemote<mojom::blink::FeatureObserver> observer_;

  absl::optional<bool> cached_allowed_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_LOCKS_LOCK_MANAGER_H_

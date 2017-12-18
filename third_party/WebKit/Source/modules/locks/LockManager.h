// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LockManager_h
#define LockManager_h

#include "bindings/modules/v8/string_or_string_sequence.h"
#include "modules/ModulesExport.h"
#include "modules/locks/LockOptions.h"
#include "platform/bindings/ScriptWrappable.h"
#include "platform/heap/HeapAllocator.h"
#include "public/platform/modules/locks/lock_manager.mojom-blink.h"

namespace blink {

class ScriptPromise;
class ScriptState;
class V8LockGrantedCallback;

class LockManager final : public ScriptWrappable,
                          public ContextLifecycleObserver {
  WTF_MAKE_NONCOPYABLE(LockManager);
  DEFINE_WRAPPERTYPEINFO();
  USING_GARBAGE_COLLECTED_MIXIN(LockManager);

 public:
  explicit LockManager(ExecutionContext*);

  ScriptPromise acquire(ScriptState*,
                        const String& name,
                        V8LockGrantedCallback*,
                        ExceptionState&);
  ScriptPromise acquire(ScriptState*,
                        const String& name,
                        const LockOptions&,
                        V8LockGrantedCallback*,
                        ExceptionState&);

  void Trace(blink::Visitor*);

  // Wrapper tracing is needed for callbacks. The reference chain is
  // NavigatorLocksImpl -> LockManager -> LockRequestImpl ->
  // V8LockGrantedCallback.
  void TraceWrappers(const ScriptWrappableVisitor*) const override;

  // Terminate all outstanding requests when the context is destroyed, since
  // this can unblock requests by other contexts.
  void ContextDestroyed(ExecutionContext*) override;

 private:
  class LockRequestImpl;

  // Track pending requests so that they can be cancelled if the context is
  // terminated.
  void AddPendingRequest(LockRequestImpl*);
  void RemovePendingRequest(LockRequestImpl*);

  HeapHashSet<TraceWrapperMember<LockRequestImpl>> pending_requests_;

  mojom::blink::LockManagerPtr service_;
};

}  // namespace blink

#endif  // LockManager_h

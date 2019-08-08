// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WAKE_LOCK_WAKE_LOCK_CONTROLLER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WAKE_LOCK_WAKE_LOCK_CONTROLLER_H_

#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/execution_context/context_lifecycle_observer.h"
#include "third_party/blink/renderer/core/page/page_visibility_observer.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/modules/wake_lock/wake_lock_type.h"
#include "third_party/blink/renderer/platform/heap/handle.h"

namespace blink {

class ScriptPromiseResolver;
class WakeLockStateRecord;

// WakeLockController is used to track per-Document wake lock state and react to
// Document changes appropriately.
class MODULES_EXPORT WakeLockController final
    : public GarbageCollectedFinalized<WakeLockController>,
      public Supplement<Document>,
      public ContextLifecycleObserver,
      public PageVisibilityObserver {
  USING_GARBAGE_COLLECTED_MIXIN(WakeLockController);

 public:
  static const char kSupplementName[];

  explicit WakeLockController(Document&);

  static WakeLockController& From(Document&);

  void Trace(blink::Visitor*) override;

  void AcquireWakeLock(WakeLockType type, ScriptPromiseResolver*);

  void ReleaseWakeLock(WakeLockType type, ScriptPromiseResolver*);

 private:
  // ContextLifecycleObserver implementation
  void ContextDestroyed(ExecutionContext*) override;

  // PageVisibilityObserver implementation
  void PageVisibilityChanged() override;

  // https://w3c.github.io/wake-lock/#concepts-and-state-record
  // Each platform wake lock (one per wake lock type) has an associated state
  // record per responsible document [...] internal slots.
  Member<WakeLockStateRecord>
      state_records_[static_cast<size_t>(WakeLockType::kMaxValue) + 1];
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WAKE_LOCK_WAKE_LOCK_CONTROLLER_H_

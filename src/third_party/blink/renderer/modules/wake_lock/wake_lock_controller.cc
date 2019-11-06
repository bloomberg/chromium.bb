// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/wake_lock/wake_lock_controller.h"

#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/modules/wake_lock/wake_lock_state_record.h"

namespace blink {

WakeLockController::WakeLockController(Document& document)
    : Supplement<Document>(document),
      ContextLifecycleObserver(&document),
      PageVisibilityObserver(document.GetPage()) {}

const char WakeLockController::kSupplementName[] = "WakeLockController";

WakeLockController& WakeLockController::From(Document& document) {
  WakeLockController* controller =
      Supplement<Document>::From<WakeLockController>(document);
  if (!controller) {
    controller = MakeGarbageCollected<WakeLockController>(document);
    ProvideTo(document, controller);
  }
  return *controller;
}

void WakeLockController::Trace(Visitor* visitor) {
  for (WakeLockStateRecord* state_record : state_records_)
    visitor->Trace(state_record);
  PageVisibilityObserver::Trace(visitor);
  ContextLifecycleObserver::Trace(visitor);
  Supplement<Document>::Trace(visitor);
}

void WakeLockController::AcquireWakeLock(WakeLockType type,
                                         ScriptPromiseResolver* resolver) {
  // Use a reference so we can create an WakeLockStateRecord object on demand.
  Member<WakeLockStateRecord>& state_record =
      state_records_[static_cast<size_t>(type)];
  if (!state_record) {
    state_record =
        MakeGarbageCollected<WakeLockStateRecord>(GetSupplementable(), type);
  }
  state_record->AcquireWakeLock(resolver);
}

void WakeLockController::ReleaseWakeLock(WakeLockType type,
                                         ScriptPromiseResolver* resolver) {
  WakeLockStateRecord* state_record = state_records_[static_cast<size_t>(type)];
  if (!state_record)
    return;
  state_record->ReleaseWakeLock(resolver);
}

void WakeLockController::ContextDestroyed(ExecutionContext*) {
  // https://w3c.github.io/wake-lock/#handling-document-loss-of-full-activity
  // 1. Let document be the responsible document of the current settings object.
  // 2. Let screenRecord be the platform wake lock's state record associated
  // with document and wake lock type "screen".
  // 3. For each lockPromise in screenRecord.[[WakeLockStateRecord]]:
  // 3.1. Run release a wake lock with lockPromise and "screen".
  // 4. Let systemRecord be the platform wake lock's state record associated
  // with document and wake lock type "system".
  // 5. For each lockPromise in systemRecord.[[WakeLockStateRecord]]:
  // 5.1. Run release a wake lock with lockPromise and "system".
  for (WakeLockStateRecord* state_record : state_records_) {
    if (state_record)
      state_record->ClearWakeLocks();
  }
}

void WakeLockController::PageVisibilityChanged() {
  // https://w3c.github.io/wake-lock/#handling-document-loss-of-visibility
  // 1. Let document be the Document of the top-level browsing context.
  // 2. If document's visibility state is "visible", abort these steps.
  if (GetPage() && GetPage()->IsPageVisible())
    return;
  // 3. Let screenRecord be the platform wake lock's state record associated
  // with wake lock type "screen".
  // 4. For each lockPromise in screenRecord.[[WakeLockStateRecord]]:
  // 4.1. Run release a wake lock with lockPromise and "screen".
  WakeLockStateRecord* state_record =
      state_records_[static_cast<size_t>(WakeLockType::kScreen)];
  if (state_record)
    state_record->ClearWakeLocks();
}

}  // namespace blink

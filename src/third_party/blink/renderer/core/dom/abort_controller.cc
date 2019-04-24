// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/dom/abort_controller.h"

#include "third_party/blink/renderer/core/dom/abort_signal.h"
#include "third_party/blink/renderer/platform/heap/visitor.h"

namespace blink {

AbortController* AbortController::Create(ExecutionContext* context) {
  return MakeGarbageCollected<AbortController>(context);
}

AbortController::AbortController(ExecutionContext* execution_context)
    : signal_(MakeGarbageCollected<AbortSignal>(execution_context)) {}

AbortController::~AbortController() = default;

void AbortController::abort() {
  signal_->SignalAbort();
}

void AbortController::Trace(Visitor* visitor) {
  visitor->Trace(signal_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink

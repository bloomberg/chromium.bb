// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/dom/AbortController.h"

#include "core/dom/AbortSignal.h"
#include "platform/heap/Visitor.h"

namespace blink {

AbortController* AbortController::Create(ExecutionContext* context) {
  return new AbortController(context);
}

AbortController::AbortController(ExecutionContext* execution_context)
    : signal_(new AbortSignal(execution_context)) {}

AbortController::~AbortController() = default;

void AbortController::abort() {
  signal_->SignalAbort();
}

void AbortController::Trace(Visitor* visitor) {
  visitor->Trace(signal_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink

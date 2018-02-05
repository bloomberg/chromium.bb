// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/dom/AbortSignal.h"

#include <utility>

#include "base/callback.h"
#include "core/dom/events/Event.h"
#include "core/event_target_names.h"
#include "core/event_type_names.h"
#include "platform/heap/Visitor.h"
#include "platform/wtf/text/AtomicString.h"

namespace blink {

AbortSignal::AbortSignal(ExecutionContext* execution_context)
    : execution_context_(execution_context) {}
AbortSignal::~AbortSignal() = default;

const AtomicString& AbortSignal::InterfaceName() const {
  return EventTargetNames::AbortSignal;
}

ExecutionContext* AbortSignal::GetExecutionContext() const {
  return execution_context_.Get();
}

void AbortSignal::AddAlgorithm(base::OnceClosure algorithm) {
  if (aborted_flag_)
    return;
  abort_algorithms_.push_back(std::move(algorithm));
}

void AbortSignal::SignalAbort() {
  if (aborted_flag_)
    return;
  aborted_flag_ = true;
  for (base::OnceClosure& closure : abort_algorithms_) {
    std::move(closure).Run();
  }
  abort_algorithms_.clear();
  DispatchEvent(Event::Create(EventTypeNames::abort));
}

void AbortSignal::Trace(Visitor* visitor) {
  visitor->Trace(execution_context_);
  EventTargetWithInlineData::Trace(visitor);
}

}  // namespace blink

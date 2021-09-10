// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/scheduler/dom_task_signal.h"

#include <utility>

#include "base/callback.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/modules/scheduler/task_priority_change_event.h"
#include "third_party/blink/renderer/modules/scheduler/task_priority_change_event_init.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"

namespace blink {

DOMTaskSignal::DOMTaskSignal(ExecutionContext* context,
                             const AtomicString& priority)
    : AbortSignal(context), priority_(priority) {}

DOMTaskSignal::~DOMTaskSignal() = default;

AtomicString DOMTaskSignal::priority() {
  return priority_;
}

void DOMTaskSignal::AddPriorityChangeAlgorithm(base::OnceClosure algorithm) {
  priority_change_algorithms_.push_back(std::move(algorithm));
}

void DOMTaskSignal::SignalPriorityChange(const AtomicString& priority,
                                         ExceptionState& exception_state) {
  if (is_priority_changing_) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNotAllowedError,
        "Cannot change priority when a prioritychange event is in progress.");
    return;
  }
  if (priority_ == priority)
    return;
  is_priority_changing_ = true;
  const AtomicString previous_priority = priority_;
  priority_ = priority;
  priority_change_status_ = PriorityChangeStatus::kPriorityHasChanged;

  for (base::OnceClosure& closure : priority_change_algorithms_) {
    std::move(closure).Run();
  }
  priority_change_algorithms_.clear();

  auto* init = TaskPriorityChangeEventInit::Create();
  init->setPreviousPriority(previous_priority);
  DispatchEvent(*TaskPriorityChangeEvent::Create(
      event_type_names::kPrioritychange, init));
  is_priority_changing_ = false;
}

void DOMTaskSignal::Trace(Visitor* visitor) const {
  AbortSignal::Trace(visitor);
}

}  // namespace blink

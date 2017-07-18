// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/timing/PerformanceObserver.h"

#include <algorithm>
#include "bindings/core/v8/ExceptionMessages.h"
#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/PerformanceObserverCallback.h"
#include "bindings/core/v8/V8BindingForCore.h"
#include "core/dom/ExecutionContext.h"
#include "core/frame/LocalDOMWindow.h"
#include "core/timing/DOMWindowPerformance.h"
#include "core/timing/Performance.h"
#include "core/timing/PerformanceEntry.h"
#include "core/timing/PerformanceObserverEntryList.h"
#include "core/timing/PerformanceObserverInit.h"
#include "platform/Timer.h"

namespace blink {

PerformanceObserver* PerformanceObserver::Create(
    ScriptState* script_state,
    PerformanceObserverCallback* callback) {
  DCHECK(IsMainThread());
  LocalDOMWindow* window = ToLocalDOMWindow(script_state->GetContext());
  if (!window) {
    V8ThrowException::ThrowTypeError(
        script_state->GetIsolate(),
        ExceptionMessages::FailedToConstruct(
            "PerformanceObserver", "No 'window' in current context."));
    return nullptr;
  }
  return new PerformanceObserver(
      script_state, DOMWindowPerformance::performance(*window), callback);
}

PerformanceObserver::PerformanceObserver(ScriptState* script_state,
                                         PerformanceBase* performance,
                                         PerformanceObserverCallback* callback)
    : execution_context_(ExecutionContext::From(script_state)),
      callback_(this, callback),
      performance_(performance),
      filter_options_(PerformanceEntry::kInvalid),
      is_registered_(false) {
  DCHECK(performance_);
}

void PerformanceObserver::observe(const PerformanceObserverInit& observer_init,
                                  ExceptionState& exception_state) {
  if (!performance_) {
    exception_state.ThrowTypeError(
        "Window may be destroyed? Performance target is invalid.");
    return;
  }

  PerformanceEntryTypeMask entry_types = PerformanceEntry::kInvalid;
  if (observer_init.hasEntryTypes() && observer_init.entryTypes().size()) {
    const Vector<String>& sequence = observer_init.entryTypes();
    for (const auto& entry_type_string : sequence)
      entry_types |= PerformanceEntry::ToEntryTypeEnum(entry_type_string);
  }
  if (entry_types == PerformanceEntry::kInvalid) {
    exception_state.ThrowTypeError(
        "A Performance Observer MUST have at least one valid entryType in its "
        "entryTypes attribute.");
    return;
  }
  filter_options_ = entry_types;
  if (is_registered_)
    performance_->UpdatePerformanceObserverFilterOptions();
  else
    performance_->RegisterPerformanceObserver(*this);
  is_registered_ = true;
}

void PerformanceObserver::disconnect() {
  if (performance_) {
    performance_->UnregisterPerformanceObserver(*this);
  }
  performance_entries_.clear();
  is_registered_ = false;
}

void PerformanceObserver::EnqueuePerformanceEntry(PerformanceEntry& entry) {
  DCHECK(IsMainThread());
  performance_entries_.push_back(&entry);
  if (performance_)
    performance_->ActivateObserver(*this);
}

bool PerformanceObserver::ShouldBeSuspended() const {
  return execution_context_->IsContextSuspended();
}

void PerformanceObserver::Deliver() {
  DCHECK(!ShouldBeSuspended());

  if (performance_entries_.IsEmpty())
    return;

  PerformanceEntryVector performance_entries;
  performance_entries.swap(performance_entries_);
  PerformanceObserverEntryList* entry_list =
      new PerformanceObserverEntryList(performance_entries);
  callback_->call(this, entry_list, this);
}

DEFINE_TRACE(PerformanceObserver) {
  visitor->Trace(execution_context_);
  visitor->Trace(callback_);
  visitor->Trace(performance_);
  visitor->Trace(performance_entries_);
}

DEFINE_TRACE_WRAPPERS(PerformanceObserver) {
  visitor->TraceWrappers(callback_);
}

}  // namespace blink

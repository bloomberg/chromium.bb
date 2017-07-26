// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/frame/ReportingObserver.h"

#include "core/dom/ExecutionContext.h"
#include "core/frame/Report.h"
#include "core/frame/ReportingContext.h"
#include "platform/bindings/ScriptState.h"

namespace blink {

ReportingObserver* ReportingObserver::Create(
    ExecutionContext* execution_context,
    ReportingObserverCallback* callback) {
  return new ReportingObserver(execution_context, callback);
}

ReportingObserver::ReportingObserver(ExecutionContext* execution_context,
                                     ReportingObserverCallback* callback)
    : execution_context_(execution_context), callback_(callback) {}

void ReportingObserver::ReportToCallback(
    const HeapVector<Member<Report>>& reports) {
  callback_->call(this, reports, this);
}

void ReportingObserver::observe() {
  ReportingContext::From(execution_context_)->RegisterObserver(this);
}

void ReportingObserver::disconnect() {
  ReportingContext::From(execution_context_)->UnregisterObserver(this);
}

DEFINE_TRACE(ReportingObserver) {
  visitor->Trace(execution_context_);
  visitor->Trace(callback_);
}

}  // namespace blink

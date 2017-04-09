// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/probe/PlatformTraceEventsAgent.h"

#include "platform/instrumentation/tracing/TraceEvent.h"
#include "platform/instrumentation/tracing/TracedValue.h"
#include "platform/probe/PlatformProbes.h"

namespace blink {

namespace {

std::unique_ptr<TracedValue> BuildData(
    const probe::PlatformSendRequest& probe) {
  std::unique_ptr<TracedValue> value = TracedValue::Create();
  value->SetString("id", String::Number(probe.identifier));
  return value;
}

}  // namespace

void PlatformTraceEventsAgent::Will(const probe::PlatformSendRequest& probe) {
  TRACE_EVENT_BEGIN1("devtools.timeline", "PlatformResourceSendRequest", "data",
                     BuildData(probe));
}

void PlatformTraceEventsAgent::Did(const probe::PlatformSendRequest& probe) {
  TRACE_EVENT_END0("devtools.timeline", "PlatformResourceSendRequest");
}

}  // namespace blink

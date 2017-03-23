// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/instrumentation/PlatformTraceEventsAgent.h"

#include "platform/instrumentation/PlatformInstrumentation.h"
#include "platform/instrumentation/tracing/TraceEvent.h"
#include "platform/instrumentation/tracing/TracedValue.h"

namespace blink {

namespace {

std::unique_ptr<TracedValue> buildData(
    const probe::PlatformSendRequest& probe) {
  std::unique_ptr<TracedValue> value = TracedValue::create();
  value->setString("id", String::number(probe.identifier));
  return value;
}

}  // namespace

void PlatformTraceEventsAgent::will(const probe::PlatformSendRequest& probe) {
  TRACE_EVENT_BEGIN1("devtools.timeline", "PlatformResourceSendRequest", "data",
                     buildData(probe));
}

void PlatformTraceEventsAgent::did(const probe::PlatformSendRequest& probe) {
  TRACE_EVENT_END0("devtools.timeline", "PlatformResourceSendRequest");
}

}  // namespace blink

// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/probe/platform_trace_events_agent.h"

#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/traced_value.h"
#include "third_party/blink/renderer/platform/probe/platform_probes.h"

namespace blink {

namespace {

}  // namespace

void PlatformTraceEventsAgent::Will(const probe::PlatformSendRequest& probe) {
}

void PlatformTraceEventsAgent::Did(const probe::PlatformSendRequest& probe) {
}

}  // namespace blink

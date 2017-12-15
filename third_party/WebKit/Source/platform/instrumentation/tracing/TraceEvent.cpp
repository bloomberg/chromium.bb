// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/instrumentation/tracing/TraceEvent.h"

#include "base/trace_event/trace_event.h"

namespace blink {
namespace TraceEvent {

void EnableTracing(const String& category_filter) {
  base::trace_event::TraceLog::GetInstance()->SetEnabled(
      base::trace_event::TraceConfig(category_filter.Utf8().data(), ""),
      base::trace_event::TraceLog::RECORDING_MODE);
}

void DisableTracing() {
  base::trace_event::TraceLog::GetInstance()->SetDisabled();
}

}  // namespace TraceEvent
}  // namespace blink

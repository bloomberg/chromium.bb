// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/base/trace_logging_activation.h"

#include <cassert>

namespace openscreen {
namespace platform {

namespace {
TraceLoggingPlatform* g_current_destination = nullptr;
}  // namespace

TraceLoggingPlatform* GetTracingDestination() {
  return g_current_destination;
}

void StartTracing(TraceLoggingPlatform* destination) {
  // TODO(crbug.com/openscreen/85): Need to revisit this to ensure thread-safety
  // around the sequencing of starting and stopping tracing.
  assert(!g_current_destination);
  g_current_destination = destination;
}

void StopTracing() {
  g_current_destination = nullptr;
}

}  // namespace platform
}  // namespace openscreen

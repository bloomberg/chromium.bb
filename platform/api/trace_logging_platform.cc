// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/api/trace_logging_platform.h"

#include "platform/api/logging.h"

namespace openscreen {
namespace platform {

TraceLoggingPlatform::~TraceLoggingPlatform() = default;

// static
TraceLoggingPlatform* TraceLoggingPlatform::GetDefaultTracingPlatform() {
  return default_platform_;
}

// static
void TraceLoggingPlatform::SetDefaultTraceLoggingPlatform(
    TraceLoggingPlatform* platform) {
  // NOTE: A DCHECK cannot be added here because BuildBots run UTs without
  // destroying static variables in between runs, so the DCHECK causes UTs to
  // fail unexpectedly.
  default_platform_ = platform;
}

// static
TraceLoggingPlatform* TraceLoggingPlatform::default_platform_ = nullptr;

}  // namespace platform
}  // namespace openscreen

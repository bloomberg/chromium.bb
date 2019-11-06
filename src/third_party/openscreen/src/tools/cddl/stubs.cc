// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// CDDL needs linked implementations of these methods when Open Screen is
// embedded, but can't link the embedder's implementation without explicit
// knowledge of it, due to the CDDL executable being linked and ran before
// the embedder's methods are compiled and linked.

#include "platform/api/time.h"
#include "platform/api/trace_logging_platform.h"

namespace openscreen {
namespace platform {
bool IsTraceLoggingEnabled(TraceCategory::Value category) {
  return false;
}

Clock::time_point Clock::now() noexcept {
  return {};
}
}  // namespace platform
}  // namespace openscreen
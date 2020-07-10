// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PLATFORM_BASE_TRACE_LOGGING_ACTIVATION_H_
#define PLATFORM_BASE_TRACE_LOGGING_ACTIVATION_H_

namespace openscreen {
namespace platform {

class TraceLoggingPlatform;

// Start or Stop trace logging. It is illegal to call StartTracing() a second
// time without having called StopTracing() to stop the prior tracing session.
void StartTracing(TraceLoggingPlatform* destination);
void StopTracing();

// If tracing is active, returns the current destination. Otherwise, returns
// nullptr.
TraceLoggingPlatform* GetTracingDestination();

}  // namespace platform
}  // namespace openscreen

#endif  // PLATFORM_BASE_TRACE_LOGGING_ACTIVATION_H_

// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PLATFORM_API_TRACE_LOGGING_PLATFORM_H_
#define PLATFORM_API_TRACE_LOGGING_PLATFORM_H_

#include "platform/api/time.h"
#include "platform/api/trace_logging_platform.h"
#include "platform/api/trace_logging_types.h"
#include "platform/base/error.h"

namespace openscreen {
namespace platform {

#define TRACE_SET_DEFAULT_PLATFORM(new_platform)                              \
  openscreen::platform::TraceLoggingPlatform::SetDefaultTraceLoggingPlatform( \
      new_platform)

// Platform API base class. The platform will be an extension of this
// method, with an instantiation of that class being provided to the below
// TracePlatformWrapper class.
class TraceLoggingPlatform {
 public:
  virtual ~TraceLoggingPlatform() = 0;

  // Returns a static TraceLoggingPlatform to be used when generating trace
  // logs.
  static TraceLoggingPlatform* GetDefaultTracingPlatform();
  static void SetDefaultTraceLoggingPlatform(TraceLoggingPlatform* platform);

  // Log a synchronous trace.
  virtual void LogTrace(const char* name,
                        const uint32_t line,
                        const char* file,
                        Clock::time_point start_time,
                        Clock::time_point end_time,
                        TraceIdHierarchy ids,
                        Error::Code error) = 0;

  // Log an asynchronous trace start.
  virtual void LogAsyncStart(const char* name,
                             const uint32_t line,
                             const char* file,
                             Clock::time_point timestamp,
                             TraceIdHierarchy ids) = 0;

  // Log an asynchronous trace end.
  virtual void LogAsyncEnd(const uint32_t line,
                           const char* file,
                           Clock::time_point timestamp,
                           TraceId trace_id,
                           Error::Code error) = 0;

 private:
  static TraceLoggingPlatform* default_platform_;
};

// Determines whether trace logging is enabled for the given category.
bool IsTraceLoggingEnabled(TraceCategory::Value category);

}  // namespace platform
}  // namespace openscreen

#endif  // PLATFORM_API_TRACE_LOGGING_PLATFORM_H_

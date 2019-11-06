// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PLATFORM_API_INTERNAL_TRACE_LOGGING_MACROS_INTERNAL_H_
#define PLATFORM_API_INTERNAL_TRACE_LOGGING_MACROS_INTERNAL_H_

#include "platform/api/internal/trace_logging_internal.h"
#include "platform/api/trace_logging_types.h"

// Using statements to simplify the below macros.
using openscreen::TraceCategory;
using openscreen::platform::internal::AsynchronousTraceLogger;
using openscreen::platform::internal::SynchronousTraceLogger;
using openscreen::platform::internal::TraceIdSetter;
using openscreen::platform::internal::TraceInstanceHelper;

namespace openscreen {

// Helper macros. These are used to simplify the macros below.
// NOTE: These cannot be #undef'd or they will stop working outside this file.
// NOTE: Two of these below macros are intentionally the same. This is to work
// around optimizations in the C++ Precompiler.
#define TRACE_INTERNAL_CONCAT(a, b) a##b
#define TRACE_INTERNAL_CONCAT_CONST(a, b) TRACE_INTERNAL_CONCAT(a, b)
#define TRACE_INTERNAL_UNIQUE_VAR_NAME(a) \
  TRACE_INTERNAL_CONCAT_CONST(a, __LINE__)

// Because we need to suppress unused variables, and this code is used
// repeatedly in below macros, define helper macros to do this on a per-compiler
// basis until we begin using C++ 17 which supports [[maybe_unused]] officially.
#if defined(__clang__)
#define TRACE_INTERNAL_IGNORE_UNUSED_VAR [[maybe_unused]]
#elif defined(__GNUC__)
#define TRACE_INTERNAL_IGNORE_UNUSED_VAR __attribute__((unused))
#else
#define TRACE_INTERNAL_IGNORE_UNUSED_VAR [[maybe_unused]]
#endif  // defined(__clang__)

// Define a macro to check if tracing is enabled or not for testing and
// compilation reasons.
#ifndef TRACE_FORCE_ENABLE
#define TRACE_IS_ENABLED(category) \
  openscreen::platform::IsTraceLoggingEnabled(TraceCategory::Value::Any)
#ifndef ENABLE_TRACE_LOGGING
#define TRACE_FORCE_DISABLE true
#endif  // ENABLE_TRACE_LOGGING
#else   // TRACE_FORCE_ENABLE defined
#define TRACE_IS_ENABLED(category) true
#endif

// Internal logging macros.
#define TRACE_SET_HIERARCHY_INTERNAL(line, ids)                          \
  alignas(32) uint8_t TRACE_INTERNAL_CONCAT_CONST(                       \
      tracing_storage,                                                   \
      line)[sizeof(openscreen::platform::internal::TraceIdSetter)];      \
  TRACE_INTERNAL_IGNORE_UNUSED_VAR                                       \
  const auto TRACE_INTERNAL_UNIQUE_VAR_NAME(trace_ref_) =                \
      TRACE_IS_ENABLED(TraceCategory::Value::Any)                        \
          ? TraceInstanceHelper<TraceIdSetter>::Create(                  \
                TRACE_INTERNAL_CONCAT_CONST(tracing_storage, line), ids) \
          : TraceInstanceHelper<TraceIdSetter>::Empty()

#define TRACE_SCOPED_INTERNAL(line, category, name, ...)                      \
  alignas(32) uint8_t TRACE_INTERNAL_CONCAT_CONST(                            \
      tracing_storage,                                                        \
      line)[sizeof(openscreen::platform::internal::SynchronousTraceLogger)];  \
  TRACE_INTERNAL_IGNORE_UNUSED_VAR                                            \
  const auto TRACE_INTERNAL_UNIQUE_VAR_NAME(trace_ref_) =                     \
      TRACE_IS_ENABLED(category)                                              \
          ? TraceInstanceHelper<SynchronousTraceLogger>::Create(              \
                TRACE_INTERNAL_CONCAT_CONST(tracing_storage, line), category, \
                name, __FILE__, __LINE__, ##__VA_ARGS__)                      \
          : TraceInstanceHelper<SynchronousTraceLogger>::Empty()

#define TRACE_ASYNC_START_INTERNAL(line, category, name, ...)                 \
  alignas(32) uint8_t TRACE_INTERNAL_CONCAT_CONST(                            \
      temp_storage,                                                           \
      line)[sizeof(openscreen::platform::internal::AsynchronousTraceLogger)]; \
  TRACE_INTERNAL_IGNORE_UNUSED_VAR                                            \
  const auto TRACE_INTERNAL_UNIQUE_VAR_NAME(trace_ref_) =                     \
      TRACE_IS_ENABLED(category)                                              \
          ? TraceInstanceHelper<AsynchronousTraceLogger>::Create(             \
                TRACE_INTERNAL_CONCAT_CONST(temp_storage, line), category,    \
                name, __FILE__, __LINE__, ##__VA_ARGS__)                      \
          : TraceInstanceHelper<AsynchronousTraceLogger>::Empty()

}  // namespace openscreen

#endif  // PLATFORM_API_INTERNAL_TRACE_LOGGING_MACROS_INTERNAL_H_

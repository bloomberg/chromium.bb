// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PLATFORM_API_TRACE_LOGGING_H_
#define PLATFORM_API_TRACE_LOGGING_H_

#include "platform/api/internal/trace_logging_internal.h"
#include "platform/api/internal/trace_logging_macros_internal.h"
#include "platform/api/trace_logging_types.h"

namespace openscreen {

// All compile-time macros for tracing.
// NOTE: The ternary operator is used here to ensure that the TraceLogger object
// is only constructed if tracing is enabled, but at the same time is created in
// the caller's scope. The C++ standards guide guarantees that the constructor
// should only be called when IsTraceLoggingEnabled(...) evaluates to true.
// static_cast calls are used because if the type of the result of the ternary
// operator does not match the expected type, temporary storage is used for the
// created object, which results in an extra call to the constructor and
// destructor of the tracing objects.
//
// Further details about how these macros are used can be found in
// docs/trace_logging.md.
// TODO(rwkeane): Add support for user-provided properties.

#ifndef TRACE_FORCE_DISABLE
#define TRACE_SET_RESULT(result)                                        \
  do {                                                                  \
    if (TRACE_IS_ENABLED(TraceCategory::Value::Any)) {                  \
      openscreen::platform::internal::ScopedTraceOperation::set_result( \
          result);                                                      \
    }                                                                   \
  } while (false)
#define TRACE_SET_HIERARCHY(ids) TRACE_SET_HIERARCHY_INTERNAL(__LINE__, ids)
#define TRACE_HIERARCHY                                                    \
  (TRACE_IS_ENABLED(TraceCategory::Value::Any)                             \
       ? openscreen::platform::internal::ScopedTraceOperation::hierarchy() \
       : TraceIdHierarchy::Empty())
#define TRACE_CURRENT_ID                                                    \
  (TRACE_IS_ENABLED(TraceCategory::Value::Any)                              \
       ? openscreen::platform::internal::ScopedTraceOperation::current_id() \
       : kEmptyTraceId)
#define TRACE_ROOT_ID                                                    \
  (TRACE_IS_ENABLED(TraceCategory::Value::Any)                           \
       ? openscreen::platform::internal::ScopedTraceOperation::root_id() \
       : kEmptyTraceId)

// Synchronous Trace Macro.
#define TRACE_SCOPED(category, name, ...) \
  TRACE_SCOPED_INTERNAL(__LINE__, category, name, ##__VA_ARGS__)

// Asynchronous Trace Macros.
#define TRACE_ASYNC_START(category, name, ...) \
  TRACE_ASYNC_START_INTERNAL(__LINE__, category, name, ##__VA_ARGS__)

#define TRACE_ASYNC_END(category, id, result)                 \
  TRACE_IS_ENABLED(category)                                  \
  ? openscreen::platform::internal::TraceBase::TraceAsyncEnd( \
        __LINE__, __FILE__, id, result)                       \
  : false

#else  // TRACE_FORCE_DISABLE defined
#define TRACE_SET_RESULT(result)
#define TRACE_SET_HIERARCHY(ids)
#define TRACE_HIERARCHY TraceIdHierarchy::Empty()
#define TRACE_CURRENT_ID kEmptyTraceId
#define TRACE_ROOT_ID kEmptyTraceId
#define TRACE_SCOPED(category, name, ...)
#define TRACE_ASYNC_START(category, name, ...)
#define TRACE_ASYNC_END(category, id, result) false
#endif  // TRACE_FORCE_DISABLE

}  // namespace openscreen

#endif  // PLATFORM_API_TRACE_LOGGING_H_

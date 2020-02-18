// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PLATFORM_API_TRACE_LOGGING_TYPES_H_
#define PLATFORM_API_TRACE_LOGGING_TYPES_H_

#include "absl/types/optional.h"

namespace openscreen {

// Define TraceId type here since other TraceLogging files import it.
using TraceId = uint64_t;

// kEmptyTraceId is the Trace ID when tracing at a global level, not inside any
// tracing block - ie this will be the parent ID for a top level tracing block.
constexpr TraceId kEmptyTraceId = 0x0;

// kUnsetTraceId is the Trace ID passed in to the tracing library when no user-
// specified value is desired.
constexpr TraceId kUnsetTraceId = std::numeric_limits<TraceId>::max();

// A class to represent the current TraceId Hirearchy and for the user to
// pass around as needed.
struct TraceIdHierarchy {
  TraceId current;
  TraceId parent;
  TraceId root;

  static constexpr TraceIdHierarchy Empty() {
    return {kEmptyTraceId, kEmptyTraceId, kEmptyTraceId};
  }

  bool HasCurrent() { return current != kUnsetTraceId; }
  bool HasParent() { return parent != kUnsetTraceId; }
  bool HasRoot() { return root != kUnsetTraceId; }
};

// BitFlags to represent the supported tracing categories.
// NOTE: These are currently placeholder values and later changes should feel
// free to edit them.
struct TraceCategory {
  enum Value : uint64_t {
    Any = std::numeric_limits<uint64_t>::max(),
    CastPlatformLayer = 0x01,
    CastStreaming = 0x01 << 1,
    CastFlinging = 0x01 << 2
  };
};

}  // namespace openscreen

#endif  // PLATFORM_API_TRACE_LOGGING_TYPES_H_

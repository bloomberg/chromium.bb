// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TraceEvent_h
#define TraceEvent_h

#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "platform/PlatformExport.h"
#include "platform/wtf/text/CString.h"
#include "platform/wtf/text/WTFString.h"

namespace WTF {

// CString version of SetTraceValue so that trace arguments can be strings.
static inline void SetTraceValue(const CString& arg,
                                 unsigned char* type,
                                 unsigned long long* value) {
  trace_event_internal::TraceValueUnion type_value;
  type_value.as_string = arg.data();
  *type = TRACE_VALUE_TYPE_COPY_STRING;
  *value = type_value.as_uint;
}

}  // namespace WTF

namespace blink {
namespace TraceEvent {

using base::trace_event::TraceScopedTrackableObject;

inline base::TimeTicks ToTraceTimestamp(double seconds) {
  return base::TimeTicks() + base::TimeDelta::FromSecondsD(seconds);
}

// This is to avoid error of passing a chromium time internal value.
void ToTraceTimestamp(int64_t);

PLATFORM_EXPORT void EnableTracing(const String& category_filter);
PLATFORM_EXPORT void DisableTracing();

}  // namespace TraceEvent
}  // namespace blink

#endif

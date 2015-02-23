// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "trace_after_dispatch_impl_error.h"

namespace blink {

void TraceAfterDispatchInlinedBase::trace(Visitor* visitor) {
  // Implement a simple form of manual dispatching, because BlinkGCPlugin gets
  // angry if dispatching statements are missing.
  //
  // This function has to be implemented out-of-line, since we need to know the
  // definition of derived classes here.
  if (tag_ == DERIVED) {
    static_cast<TraceAfterDispatchInlinedDerived*>(this)->traceAfterDispatch(
        visitor);
  } else {
    traceAfterDispatch(visitor);
  }
}

void TraceAfterDispatchExternBase::trace(Visitor* visitor) {
  if (tag_ == DERIVED) {
    static_cast<TraceAfterDispatchExternDerived*>(this)->traceAfterDispatch(
        visitor);
  } else {
    traceAfterDispatch(visitor);
  }
}

void TraceAfterDispatchExternBase::traceAfterDispatch(Visitor* visitor) {
  traceAfterDispatchImpl(visitor);
}

template <typename VisitorDispatcher>
inline void TraceAfterDispatchExternBase::traceAfterDispatchImpl(
    VisitorDispatcher visitor) {
  // No trace call.
}

void TraceAfterDispatchExternDerived::traceAfterDispatch(Visitor* visitor) {
  traceAfterDispatchImpl(visitor);
}

template <typename VisitorDispatcher>
inline void TraceAfterDispatchExternDerived::traceAfterDispatchImpl(
    VisitorDispatcher visitor) {
  // Ditto.
}

}

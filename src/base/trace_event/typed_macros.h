// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_TRACE_EVENT_TYPED_MACROS_H_
#define BASE_TRACE_EVENT_TYPED_MACROS_H_

#include "base/base_export.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/typed_macros_internal.h"
#include "build/build_config.h"

// Needed not for this file, but for every user of the TRACE_EVENT macros for
// the lambda definition. So included here for convenience.
#include "third_party/perfetto/include/perfetto/tracing/event_context.h"
#include "third_party/perfetto/protos/perfetto/trace/track_event/track_event.pbzero.h"

#if defined(TRACE_EVENT_BEGIN)
#error "Another copy of perfetto tracing macros have been included"
#endif

// This file implements typed event macros [1] that will be provided by the
// Perfetto client library in the future, as a stop-gap to support typed trace
// events in Chrome until we are ready to switch to the client library's
// implementation of trace macros.
// [1] https://perfetto.dev/docs/instrumentation/track-events
// TODO(crbug/1006541): Replace this file with the Perfetto client library.

// Begin a thread-scoped slice under |category| with the title |name|. Both
// strings must be static constants. The track event is only recorded if
// |category| is enabled for a tracing session.
//
// Rest of parameters can contain: a perfetto::Track object for asynchronous
// events and a lambda used to fill typed event. Should be passed in that exact
// order when both are used.
//
// When lambda is passed as an argument, it is executed synchronously.
//
// TODO(nuskos): Give a simple example once we have a typed event that doesn't
// need interning.
//   TRACE_EVENT_BEGIN("log", "LogMessage",
//       [](perfetto::EventContext ctx) {
//           auto* event = ctx.event();
//           // Fill in some field in track_event.
//       });
#define TRACE_EVENT_BEGIN(category, name, ...)                              \
  TRACING_INTERNAL_ADD_TRACE_EVENT(TRACE_EVENT_PHASE_BEGIN, category, name, \
                                   TRACE_EVENT_FLAG_NONE, ##__VA_ARGS__)

// End a thread-scoped slice under |category|.
#define TRACE_EVENT_END(category, ...)                                       \
  TRACING_INTERNAL_ADD_TRACE_EVENT(TRACE_EVENT_PHASE_END, category,          \
                                   trace_event_internal::kTraceEventEndName, \
                                   TRACE_EVENT_FLAG_NONE, ##__VA_ARGS__)

// Begin a thread-scoped slice which gets automatically closed when going out
// of scope.
//
// BEWARE: similarly to TRACE_EVENT_BEGIN, this macro does accept a track, but
// it does not work properly and should not be used.
// TODO(b/154583431): figure out how to fix or disallow that and update the
// comment.
//
// Similarly to TRACE_EVENT_BEGIN, when lambda is passed as an argument, it is
// executed synchronously.
#define TRACE_EVENT(category, name, ...) \
  TRACING_INTERNAL_SCOPED_ADD_TRACE_EVENT(category, name, ##__VA_ARGS__)

// Emit a single event called "name" immediately, with zero duration.
#define TRACE_EVENT_INSTANT(category, name, scope, ...)                       \
  TRACING_INTERNAL_ADD_TRACE_EVENT(TRACE_EVENT_PHASE_INSTANT, category, name, \
                                   scope, ##__VA_ARGS__)

#endif  // BASE_TRACE_EVENT_TYPED_MACROS_H_

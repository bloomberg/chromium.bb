// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_TRACING_PUBLIC_CPP_PERFETTO_TRACK_EVENT_THREAD_LOCAL_EVENT_SINK_H_
#define SERVICES_TRACING_PUBLIC_CPP_PERFETTO_TRACK_EVENT_THREAD_LOCAL_EVENT_SINK_H_

#include <cstdint>
#include <memory>
#include <string>
#include <utility>

#include "base/component_export.h"
#include "base/time/time.h"
#include "services/tracing/public/cpp/perfetto/interning_index.h"
#include "services/tracing/public/cpp/perfetto/thread_local_event_sink.h"
#include "third_party/perfetto/include/perfetto/ext/tracing/core/trace_writer.h"
#include "third_party/perfetto/include/perfetto/protozero/message_handle.h"

namespace perfetto {
class StartupTraceWriter;
}  // namespace perfetto

namespace tracing {

// ThreadLocalEventSink that emits TrackEvent protos.
class COMPONENT_EXPORT(TRACING_CPP) TrackEventThreadLocalEventSink
    : public ThreadLocalEventSink {
 public:
  TrackEventThreadLocalEventSink(
      std::unique_ptr<perfetto::StartupTraceWriter> trace_writer,
      uint32_t session_id,
      bool disable_interning,
      bool proto_writer_filtering_enabled);
  ~TrackEventThreadLocalEventSink() override;

  // Resets emitted incremental state on all threads and causes incremental data
  // (e.g. interning index entries and a ThreadDescriptor) to be emitted again.
  static void ClearIncrementalState();

  // ThreadLocalEventSink implementation:
  void AddTraceEvent(base::trace_event::TraceEvent* trace_event,
                     base::trace_event::TraceEventHandle* handle) override;
  void UpdateDuration(base::trace_event::TraceEventHandle handle,
                      const base::TimeTicks& now,
                      const base::ThreadTicks& thread_now) override;
  void Flush() override;

 private:
  static constexpr size_t kMaxCompleteEventDepth = 30;

  // TODO(eseckler): Make it possible to register new indexes for use from
  // TRACE_EVENT macros.
  InterningIndex<const char*> interned_event_categories_;
  InterningIndex<const char*, std::string> interned_event_names_;
  InterningIndex<const char*, std::string> interned_annotation_names_;
  InterningIndex<std::pair<const char*, const char*>>
      interned_source_locations_;

  static std::atomic<uint32_t> incremental_state_reset_id_;

  bool reset_incremental_state_ = true;
  uint32_t last_incremental_state_reset_id_ = 0;
  base::TimeTicks last_timestamp_;
  base::ThreadTicks last_thread_time_;
  int process_id_;
  int thread_id_;

  base::trace_event::TraceEvent complete_event_stack_[kMaxCompleteEventDepth];
  uint32_t current_stack_depth_ = 0;

  const bool privacy_filtering_enabled_;
};

}  // namespace tracing

#endif  // SERVICES_TRACING_PUBLIC_CPP_PERFETTO_TRACK_EVENT_THREAD_LOCAL_EVENT_SINK_H_

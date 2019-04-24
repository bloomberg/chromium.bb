// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_TRACING_PUBLIC_CPP_PERFETTO_CHROME_BUNDLE_THREAD_LOCAL_EVENT_SINK_H_
#define SERVICES_TRACING_PUBLIC_CPP_PERFETTO_CHROME_BUNDLE_THREAD_LOCAL_EVENT_SINK_H_

#include <cstdint>
#include <map>
#include <memory>

#include "base/component_export.h"
#include "base/trace_event/trace_event.h"
#include "services/tracing/public/cpp/perfetto/thread_local_event_sink.h"
#include "third_party/perfetto/include/perfetto/protozero/message_handle.h"
#include "third_party/perfetto/include/perfetto/tracing/core/trace_writer.h"

namespace perfetto {
class StartupTraceWriter;
namespace protos {
namespace pbzero {
class ChromeEventBundle;
class ChromeTraceEvent_Arg;
}  // namespace pbzero
}  // namespace protos
}  // namespace perfetto

namespace tracing {

// ThreadLocalEventSink that emits ChromeEventBundle protos.
// TODO(eseckler): Remove once we switched to new proto TraceEvent format.
class COMPONENT_EXPORT(TRACING_CPP) ChromeBundleThreadLocalEventSink
    : public ThreadLocalEventSink {
 public:
  ChromeBundleThreadLocalEventSink(
      std::unique_ptr<perfetto::StartupTraceWriter> trace_writer,
      uint32_t session_id,
      bool disable_interning,
      bool thread_will_flush);
  ~ChromeBundleThreadLocalEventSink() override;

  // ThreadLocalEventSink implementation:
  void AddTraceEvent(base::trace_event::TraceEvent* trace_event,
                     base::trace_event::TraceEventHandle* handle) override;
  void UpdateDuration(base::trace_event::TraceEventHandle handle,
                      const base::TimeTicks& now,
                      const base::ThreadTicks& thread_now) override;
  void Flush() override;

 private:
  using ChromeEventBundleHandle =
      protozero::MessageHandle<perfetto::protos::pbzero::ChromeEventBundle>;

  void EnsureValidHandles();
  int GetStringTableIndexForString(const char* str_value);
  void AddConvertableToTraceFormat(
      base::trace_event::ConvertableToTraceFormat* value,
      perfetto::protos::pbzero::ChromeTraceEvent_Arg* arg);

  static constexpr size_t kMaxCompleteEventDepth = 20;

  const bool thread_will_flush_;
  ChromeEventBundleHandle event_bundle_;
  perfetto::TraceWriter::TracePacketHandle trace_packet_handle_;
  std::map<intptr_t, int> string_table_;
  int next_string_table_index_ = 0;
  size_t current_eventcount_for_message_ = 0;
  base::trace_event::TraceEvent complete_event_stack_[kMaxCompleteEventDepth];
  uint32_t current_stack_depth_ = 0;
};

}  // namespace tracing

#endif  // SERVICES_TRACING_PUBLIC_CPP_PERFETTO_CHROME_BUNDLE_THREAD_LOCAL_EVENT_SINK_H_

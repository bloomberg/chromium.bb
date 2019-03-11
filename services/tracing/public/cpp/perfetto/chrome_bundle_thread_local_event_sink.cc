// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/tracing/public/cpp/perfetto/chrome_bundle_thread_local_event_sink.h"

#include <string>
#include <utility>

#include "base/trace_event/common/trace_event_common.h"
#include "base/trace_event/trace_buffer.h"
#include "base/trace_event/trace_log.h"
#include "build/build_config.h"
#include "services/tracing/public/cpp/perfetto/traced_value_proto_writer.h"
#include "third_party/perfetto/include/perfetto/tracing/core/startup_trace_writer.h"
#include "third_party/perfetto/protos/perfetto/trace/chrome/chrome_trace_event.pbzero.h"
#include "third_party/perfetto/protos/perfetto/trace/trace_packet.pbzero.h"

using TraceLog = base::trace_event::TraceLog;

namespace tracing {

namespace {

constexpr size_t kMaxEventsPerMessage = 100;

// To mark TraceEvent handles that have been added by Perfetto,
// we use the chunk index so high that TraceLog would've asserted
// at this point anyway.
constexpr uint32_t kMagicChunkIndex =
    base::trace_event::TraceBufferChunk::kMaxChunkIndex;

}  // namespace

// static
constexpr size_t ChromeBundleThreadLocalEventSink::kMaxCompleteEventDepth;

ChromeBundleThreadLocalEventSink::ChromeBundleThreadLocalEventSink(
    std::unique_ptr<perfetto::StartupTraceWriter> trace_writer,
    uint32_t session_id,
    bool disable_interning,
    bool thread_will_flush)
    : ThreadLocalEventSink(std::move(trace_writer),
                           session_id,
                           disable_interning),
      thread_will_flush_(thread_will_flush) {}

ChromeBundleThreadLocalEventSink::~ChromeBundleThreadLocalEventSink() {}

void ChromeBundleThreadLocalEventSink::EnsureValidHandles() {
  if (trace_packet_handle_) {
    return;
  }

  trace_packet_handle_ = trace_writer_->NewTracePacket();
  event_bundle_ =
      ChromeEventBundleHandle(trace_packet_handle_->set_chrome_events());
  string_table_.clear();
  next_string_table_index_ = 0;
  current_eventcount_for_message_ = 0;
}

int ChromeBundleThreadLocalEventSink::GetStringTableIndexForString(
    const char* str_value) {
  EnsureValidHandles();

  auto it = string_table_.find(reinterpret_cast<intptr_t>(str_value));
  if (it != string_table_.end()) {
    CHECK_EQ(std::string(reinterpret_cast<const char*>(it->first)),
             std::string(str_value));

    return it->second;
  }

  int string_table_index = ++next_string_table_index_;
  string_table_[reinterpret_cast<intptr_t>(str_value)] = string_table_index;

  auto* new_string_table_entry = event_bundle_->add_string_table();
  new_string_table_entry->set_value(str_value);
  new_string_table_entry->set_index(string_table_index);

  return string_table_index;
}

void ChromeBundleThreadLocalEventSink::AddConvertableToTraceFormat(
    base::trace_event::ConvertableToTraceFormat* value,
    perfetto::protos::pbzero::ChromeTraceEvent_Arg* arg) {
  PerfettoProtoAppender proto_appender(arg);
  if (value->AppendToProto(&proto_appender)) {
    return;
  }

  std::string json = value->ToString();
  arg->set_json_value(json.c_str());
}

void ChromeBundleThreadLocalEventSink::AddTraceEvent(
    base::trace_event::TraceEvent* trace_event,
    base::trace_event::TraceEventHandle* handle) {
  if (handle && trace_event->phase() == TRACE_EVENT_PHASE_COMPLETE) {
    // 'X' phase events are added through a scoped object and
    // will have its duration updated when said object drops off
    // the stack; keep a copy of the event around instead of
    // writing it into SHM, until we have the duration.
    // We can't keep the TraceEvent around in the scoped object
    // itself as that causes a lot more codegen in the callsites
    // and bloats the binary size too much (due to the increased
    // sizeof() of the scoped object itself).
    DCHECK_LT(current_stack_depth_, kMaxCompleteEventDepth);
    if (current_stack_depth_ >= kMaxCompleteEventDepth) {
      return;
    }

    complete_event_stack_[current_stack_depth_] = std::move(*trace_event);
    handle->event_index = ++current_stack_depth_;
    handle->chunk_index = kMagicChunkIndex;
    handle->chunk_seq = session_id_;
    return;
  }

  EnsureValidHandles();

  uint32_t name_index = 0;
  uint32_t category_name_index = 0;
  const size_t kMaxSize = base::trace_event::TraceArguments::kMaxSize;
  uint32_t arg_name_indices[kMaxSize] = {0};

  // By default, we bundle multiple events into a single TracePacket, e.g. to
  // avoid repeating strings by interning them into a string table. However,
  // we shouldn't bundle events in two situations:
  // 1) If the thread we're executing on is unable to flush events on demand
  //    (threads without a MessageLoop), the service will only be able to
  //    recover completed TracePackets. For these threads, bundling events
  //    would increase the number of events that are lost at the end of
  //    tracing.
  // 2) During startup tracing, the StartupTraceWriter buffers TracePackets in
  //    a temporary local buffer until it is bound to the SMB when it becomes
  //    available. While a TracePacket is written to this temporary buffer,
  //    the writer cannot be bound to the SMB. Bundling events would increase
  //    the time during which binding the writer is blocked.
  bool bundle_events = thread_will_flush_ && trace_writer_->was_bound();

  // Populate any new string table parts first; has to be done before
  // the add_trace_events() call (as the string table is part of the outer
  // proto message).
  // If the TRACE_EVENT_FLAG_COPY flag is set, the char* pointers aren't
  // necessarily valid after the TRACE_EVENT* call, and so we need to store
  // the string every time.
  bool string_table_enabled = !(trace_event->flags() & TRACE_EVENT_FLAG_COPY) &&
                              bundle_events && !disable_interning_;
  if (string_table_enabled) {
    name_index = GetStringTableIndexForString(trace_event->name());
    category_name_index = GetStringTableIndexForString(
        TraceLog::GetCategoryGroupName(trace_event->category_group_enabled()));

    for (size_t i = 0; i < trace_event->arg_size() && trace_event->arg_name(i);
         ++i) {
      arg_name_indices[i] =
          GetStringTableIndexForString(trace_event->arg_name(i));
    }
  }

  auto* new_trace_event = event_bundle_->add_trace_events();

  if (name_index) {
    new_trace_event->set_name_index(name_index);
  } else {
    new_trace_event->set_name(trace_event->name());
  }

  if (category_name_index) {
    new_trace_event->set_category_group_name_index(category_name_index);
  } else {
    new_trace_event->set_category_group_name(
        TraceLog::GetCategoryGroupName(trace_event->category_group_enabled()));
  }

  new_trace_event->set_timestamp(
      trace_event->timestamp().since_origin().InMicroseconds());

  uint32_t flags = trace_event->flags();
  new_trace_event->set_flags(flags);

  int process_id;
  int thread_id;
  if ((flags & TRACE_EVENT_FLAG_HAS_PROCESS_ID) &&
      trace_event->process_id() != base::kNullProcessId) {
    process_id = trace_event->process_id();
    thread_id = -1;
  } else {
    process_id = TraceLog::GetInstance()->process_id();
    thread_id = trace_event->thread_id();
  }

  new_trace_event->set_process_id(process_id);
  new_trace_event->set_thread_id(thread_id);

  char phase = trace_event->phase();
  new_trace_event->set_phase(phase);

  for (size_t i = 0; i < trace_event->arg_size() && trace_event->arg_name(i);
       ++i) {
    auto type = trace_event->arg_type(i);
    auto* new_arg = new_trace_event->add_args();

    if (arg_name_indices[i]) {
      new_arg->set_name_index(arg_name_indices[i]);
    } else {
      new_arg->set_name(trace_event->arg_name(i));
    }

    if (type == TRACE_VALUE_TYPE_CONVERTABLE) {
      AddConvertableToTraceFormat(trace_event->arg_convertible_value(i),
                                  new_arg);
      continue;
    }

    auto& value = trace_event->arg_value(i);
    switch (type) {
      case TRACE_VALUE_TYPE_BOOL:
        new_arg->set_bool_value(value.as_bool);
        break;
      case TRACE_VALUE_TYPE_UINT:
        new_arg->set_uint_value(value.as_uint);
        break;
      case TRACE_VALUE_TYPE_INT:
        new_arg->set_int_value(value.as_int);
        break;
      case TRACE_VALUE_TYPE_DOUBLE:
        new_arg->set_double_value(value.as_double);
        break;
      case TRACE_VALUE_TYPE_POINTER:
        new_arg->set_pointer_value(static_cast<uint64_t>(
            reinterpret_cast<uintptr_t>(value.as_pointer)));
        break;
      case TRACE_VALUE_TYPE_STRING:
      case TRACE_VALUE_TYPE_COPY_STRING:
        new_arg->set_string_value(value.as_string ? value.as_string : "NULL");
        break;
      default:
        NOTREACHED() << "Don't know how to print this value";
        break;
    }
  }

  if (phase == TRACE_EVENT_PHASE_COMPLETE) {
    new_trace_event->set_duration(trace_event->duration().InMicroseconds());

    if (!trace_event->thread_timestamp().is_null()) {
      int64_t thread_duration = trace_event->thread_duration().InMicroseconds();
      if (thread_duration != -1) {
        new_trace_event->set_thread_duration(thread_duration);
      }
    }
  }

  if (!trace_event->thread_timestamp().is_null()) {
    int64_t thread_time_int64 =
        trace_event->thread_timestamp().since_origin().InMicroseconds();
    new_trace_event->set_thread_timestamp(thread_time_int64);
  }

  if (trace_event->scope() != trace_event_internal::kGlobalScope) {
    new_trace_event->set_scope(trace_event->scope());
  }

  if (flags & (TRACE_EVENT_FLAG_HAS_ID | TRACE_EVENT_FLAG_HAS_LOCAL_ID |
               TRACE_EVENT_FLAG_HAS_GLOBAL_ID)) {
    new_trace_event->set_id(trace_event->id());
  }

  if ((flags & TRACE_EVENT_FLAG_FLOW_OUT) ||
      (flags & TRACE_EVENT_FLAG_FLOW_IN)) {
    new_trace_event->set_bind_id(trace_event->bind_id());
  }

  // See comment for |bundle_events| above. We also enforce an upper bound on
  // how many submessages we'll add for a given TracePacket so they won't grow
  // infinitely.
  if (!bundle_events ||
      current_eventcount_for_message_++ > kMaxEventsPerMessage) {
    event_bundle_ = ChromeEventBundleHandle();
    trace_packet_handle_ = perfetto::TraceWriter::TracePacketHandle();
  }
}

void ChromeBundleThreadLocalEventSink::UpdateDuration(
    base::trace_event::TraceEventHandle handle,
    const base::TimeTicks& now,
    const base::ThreadTicks& thread_now) {
  if (!handle.event_index || handle.chunk_index != kMagicChunkIndex ||
      handle.chunk_seq != session_id_) {
    return;
  }

  DCHECK_GE(current_stack_depth_, 1u);
  // During trace shutdown, as the list of enabled categories are
  // non-monotonically shut down, there's the possibility that events higher in
  // the stack will have their category disabled prior to events lower in the
  // stack, hence we get misaligned here. In this case, as we know we're
  // shutting down, we leave the events unfinished.
  if (handle.event_index != current_stack_depth_) {
    DCHECK(handle.event_index > 0 &&
           handle.event_index < current_stack_depth_ &&
           !base::trace_event::TraceLog::GetInstance()->IsEnabled());
    current_stack_depth_ = handle.event_index - 1;
    return;
  }

  current_stack_depth_--;
  complete_event_stack_[current_stack_depth_].UpdateDuration(now, thread_now);
  AddTraceEvent(&complete_event_stack_[current_stack_depth_], nullptr);

#if defined(OS_ANDROID)
  complete_event_stack_[current_stack_depth_].SendToATrace();
#endif
}

void ChromeBundleThreadLocalEventSink::Flush() {
  // TODO(oysteine): This will break events if we flush
  // while recording. This can't be done on destruction
  // as this can trigger PostTasks which may not be possible
  // if the thread is being shut down.
  while (current_stack_depth_--) {
    AddTraceEvent(&complete_event_stack_[current_stack_depth_], nullptr);
  }

  event_bundle_ = ChromeEventBundleHandle();
  trace_packet_handle_ = perfetto::TraceWriter::TracePacketHandle();
  trace_writer_->Flush();
}

}  // namespace tracing

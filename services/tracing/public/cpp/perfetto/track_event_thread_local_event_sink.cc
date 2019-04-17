// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/tracing/public/cpp/perfetto/track_event_thread_local_event_sink.h"

#include "base/stl_util.h"
#include "base/trace_event/common/trace_event_common.h"
#include "base/trace_event/trace_buffer.h"
#include "base/trace_event/trace_log.h"
#include "build/build_config.h"
#include "services/tracing/public/cpp/perfetto/producer_client.h"
#include "services/tracing/public/cpp/perfetto/traced_value_proto_writer.h"
#include "third_party/perfetto/include/perfetto/tracing/core/startup_trace_writer.h"
#include "third_party/perfetto/protos/perfetto/trace/interned_data/interned_data.pbzero.h"
#include "third_party/perfetto/protos/perfetto/trace/trace_packet.pbzero.h"
#include "third_party/perfetto/protos/perfetto/trace/track_event/debug_annotation.pbzero.h"
#include "third_party/perfetto/protos/perfetto/trace/track_event/process_descriptor.pbzero.h"
#include "third_party/perfetto/protos/perfetto/trace/track_event/task_execution.pbzero.h"
#include "third_party/perfetto/protos/perfetto/trace/track_event/thread_descriptor.pbzero.h"
#include "third_party/perfetto/protos/perfetto/trace/track_event/track_event.pbzero.h"

using TraceLog = base::trace_event::TraceLog;
using TrackEvent = perfetto::protos::pbzero::TrackEvent;

namespace tracing {

namespace {

// To mark TraceEvent handles that have been added by Perfetto,
// we use the chunk index so high that TraceLog would've asserted
// at this point anyway.
constexpr uint32_t kMagicChunkIndex =
    base::trace_event::TraceBufferChunk::kMaxChunkIndex;

// Force an incremental state reset every 1000 events on each thread. This
// limits the maximum number of events we lose when trace buffers wrap.
// TODO(eseckler): Tune this value experimentally and/or replace it with a
// signal by the service.
constexpr int kMaxEventsBeforeIncrementalStateReset = 1000;

// Replacement string for names of events with TRACE_EVENT_FLAG_COPY.
const char* const kPrivacyFiltered = "PRIVACY_FILTERED";

base::ThreadTicks ThreadNow() {
  return base::ThreadTicks::IsSupported()
             ? base::subtle::ThreadTicksNowIgnoringOverride()
             : base::ThreadTicks();
}

// Names of events that should be converted into a TaskExecution event.
const char* kTaskExecutionEventCategory = "toplevel";
const char* kTaskExecutionEventNames[3] = {"ThreadControllerImpl::RunTask",
                                           "ThreadController::Task",
                                           "ThreadPool_RunTask"};

void AddConvertableToTraceFormat(
    base::trace_event::ConvertableToTraceFormat* value,
    perfetto::protos::pbzero::DebugAnnotation* annotation) {
  PerfettoProtoAppender proto_appender(annotation);
  if (value->AppendToProto(&proto_appender)) {
    return;
  }

  std::string json = value->ToString();
  annotation->set_legacy_json_value(json.c_str());
}

void WriteDebugAnnotations(
    base::trace_event::TraceEvent* trace_event,
    TrackEvent* track_event,
    InterningIndexEntry** current_packet_interning_entries) {
  for (size_t i = 0; i < trace_event->arg_size() && trace_event->arg_name(i);
       ++i) {
    auto type = trace_event->arg_type(i);
    auto* annotation = track_event->add_debug_annotations();

    annotation->set_name_iid(current_packet_interning_entries[i]->id);

    if (type == TRACE_VALUE_TYPE_CONVERTABLE) {
      AddConvertableToTraceFormat(trace_event->arg_convertible_value(i),
                                  annotation);
      continue;
    }

    auto& value = trace_event->arg_value(i);
    switch (type) {
      case TRACE_VALUE_TYPE_BOOL:
        annotation->set_bool_value(value.as_bool);
        break;
      case TRACE_VALUE_TYPE_UINT:
        annotation->set_uint_value(value.as_uint);
        break;
      case TRACE_VALUE_TYPE_INT:
        annotation->set_int_value(value.as_int);
        break;
      case TRACE_VALUE_TYPE_DOUBLE:
        annotation->set_double_value(value.as_double);
        break;
      case TRACE_VALUE_TYPE_POINTER:
        annotation->set_pointer_value(static_cast<uint64_t>(
            reinterpret_cast<uintptr_t>(value.as_pointer)));
        break;
      case TRACE_VALUE_TYPE_STRING:
      case TRACE_VALUE_TYPE_COPY_STRING:
        annotation->set_string_value(value.as_string ? value.as_string
                                                     : "NULL");
        break;
      default:
        NOTREACHED() << "Don't know how to serialize this value";
        break;
    }
  }
}

}  // namespace

// static
constexpr size_t TrackEventThreadLocalEventSink::kMaxCompleteEventDepth;

TrackEventThreadLocalEventSink::TrackEventThreadLocalEventSink(
    std::unique_ptr<perfetto::StartupTraceWriter> trace_writer,
    uint32_t session_id,
    bool disable_interning,
    bool proto_writer_filtering_enabled)
    : ThreadLocalEventSink(std::move(trace_writer),
                           session_id,
                           disable_interning),
      // TODO(eseckler): Tune these values experimentally.
      interned_event_categories_(1000),
      interned_event_names_(1000, 100),
      interned_annotation_names_(1000, 100),
      interned_source_locations_(1000),
      process_id_(TraceLog::GetInstance()->process_id()),
      thread_id_(static_cast<int>(base::PlatformThread::CurrentId())),
      privacy_filtering_enabled_(proto_writer_filtering_enabled) {}

TrackEventThreadLocalEventSink::~TrackEventThreadLocalEventSink() {}

// TODO(eseckler): Trigger this upon a signal from the perfetto, once perfetto
// supports this.
void TrackEventThreadLocalEventSink::ResetIncrementalState() {
  reset_incremental_state_ = true;
  events_since_last_incremental_state_reset_ = 0;
}

void TrackEventThreadLocalEventSink::AddTraceEvent(
    base::trace_event::TraceEvent* trace_event,
    base::trace_event::TraceEventHandle* handle) {
  // TODO(eseckler): Support splitting COMPLETE events into BEGIN/END pairs.
  // For now, we emit them as legacy events so that the generated JSON trace
  // size remains small.
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

  uint32_t flags = trace_event->flags();
  bool copy_strings = flags & TRACE_EVENT_FLAG_COPY;
  bool explicit_timestamp = flags & TRACE_EVENT_FLAG_EXPLICIT_TIMESTAMP;

  ScopedPerfettoPostTaskBlocker post_task_blocker(
      !!(flags & TRACE_EVENT_FLAG_DISALLOW_POSTTASK));

  if (reset_incremental_state_) {
    interned_event_categories_.ResetEmittedState();
    interned_event_names_.ResetEmittedState();
    interned_annotation_names_.ResetEmittedState();
    interned_source_locations_.ResetEmittedState();

    // Emit a new thread descriptor in a separate packet, where we also set
    // the |reset_incremental_state| field.
    auto trace_packet = trace_writer_->NewTracePacket();
    trace_packet->set_incremental_state_cleared(true);
    auto* thread_descriptor = trace_packet->set_thread_descriptor();
    thread_descriptor->set_pid(process_id_);
    thread_descriptor->set_tid(thread_id_);
    if (explicit_timestamp) {
      // Don't use a user-provided timestamp as a reference timestamp.
      last_timestamp_ = TRACE_TIME_TICKS_NOW();
    } else {
      last_timestamp_ = trace_event->timestamp();
    }
    if (trace_event->thread_timestamp().is_null()) {
      last_thread_time_ = ThreadNow();
    } else {
      // Thread timestamp is never user-provided.
      DCHECK(trace_event->thread_timestamp() <= ThreadNow());
      last_thread_time_ = trace_event->thread_timestamp();
    }
    thread_descriptor->set_reference_timestamp_us(
        last_timestamp_.since_origin().InMicroseconds());
    thread_descriptor->set_reference_thread_time_us(
        last_thread_time_.since_origin().InMicroseconds());
    // TODO(eseckler): Fill in remaining fields in ThreadDescriptor.

    reset_incremental_state_ = false;
  }

  const char* category_name =
      TraceLog::GetCategoryGroupName(trace_event->category_group_enabled());
  InterningIndexEntry* interned_category =
      interned_event_categories_.LookupOrAdd(category_name);

  InterningIndexEntry* interned_name;
  const size_t kMaxSize = base::trace_event::TraceArguments::kMaxSize;
  InterningIndexEntry* interned_annotation_names[kMaxSize] = {nullptr};
  InterningIndexEntry* interned_source_location = nullptr;

  if (copy_strings) {
    if (privacy_filtering_enabled_) {
      interned_name = interned_event_names_.LookupOrAdd(kPrivacyFiltered);
    } else {
      interned_name =
          interned_event_names_.LookupOrAdd(std::string(trace_event->name()));
      for (size_t i = 0;
           i < trace_event->arg_size() && trace_event->arg_name(i); ++i) {
        interned_annotation_names[i] = interned_annotation_names_.LookupOrAdd(
            std::string(trace_event->arg_name(i)));
      }
    }
  } else {
    interned_name = interned_event_names_.LookupOrAdd(trace_event->name());

    // TODO(eseckler): Remove special handling of typed events here once we
    // support them in TRACE_EVENT macros.
    if (flags & TRACE_EVENT_FLAG_TYPED_PROTO_ARGS) {
      DCHECK_EQ(strcmp(category_name, kTaskExecutionEventCategory), 0);
      DCHECK(strcmp(trace_event->name(), kTaskExecutionEventNames[0]) == 0 ||
             strcmp(trace_event->name(), kTaskExecutionEventNames[1]) == 0 ||
             strcmp(trace_event->name(), kTaskExecutionEventNames[2]) == 0);

      if (trace_event->arg_size() == 2u) {
        DCHECK_EQ(trace_event->arg_type(0), TRACE_VALUE_TYPE_STRING);
        DCHECK_EQ(trace_event->arg_type(1), TRACE_VALUE_TYPE_STRING);
        interned_source_location = interned_source_locations_.LookupOrAdd(
            std::make_pair(trace_event->arg_value(0).as_string,
                           trace_event->arg_value(1).as_string));
      } else {
        DCHECK_EQ(trace_event->arg_size(), 1u);
        DCHECK_EQ(trace_event->arg_type(0), TRACE_VALUE_TYPE_STRING);
        interned_source_location = interned_source_locations_.LookupOrAdd(
            std::make_pair(trace_event->arg_value(0).as_string,
                           static_cast<const char*>(nullptr)));
      }
    } else if (!privacy_filtering_enabled_) {
      for (size_t i = 0;
           i < trace_event->arg_size() && trace_event->arg_name(i); ++i) {
        interned_annotation_names[i] =
            interned_annotation_names_.LookupOrAdd(trace_event->arg_name(i));
      }
    }
  }

  // Start a new packet which will contain the trace event and any new
  // interning index entries.
  auto trace_packet = trace_writer_->NewTracePacket();
  auto* track_event = trace_packet->set_track_event();

  // Events for different processes/threads always use an absolute timestamp.
  bool force_absolute_timestamp =
      ((flags & TRACE_EVENT_FLAG_HAS_PROCESS_ID) &&
       trace_event->process_id() != base::kNullProcessId) ||
      thread_id_ != trace_event->thread_id() || explicit_timestamp;

  if (force_absolute_timestamp || last_timestamp_ > trace_event->timestamp()) {
    track_event->set_timestamp_absolute_us(
        trace_event->timestamp().since_origin().InMicroseconds());
  } else {
    track_event->set_timestamp_delta_us(
        (trace_event->timestamp() - last_timestamp_).InMicroseconds());
    last_timestamp_ = trace_event->timestamp();
  }

  if (!trace_event->thread_timestamp().is_null()) {
    // Thread timestamps are never user-provided, but COMPLETE events may get
    // reordered, so we can still observe timestamps that are further in the
    // past. Emit those as absolute timestamps, since we don't support
    // negative deltas.
    if (last_thread_time_ > trace_event->thread_timestamp()) {
      track_event->set_thread_time_absolute_us(
          trace_event->thread_timestamp().since_origin().InMicroseconds());
    } else {
      track_event->set_thread_time_delta_us(
          (trace_event->thread_timestamp() - last_thread_time_)
              .InMicroseconds());
      last_thread_time_ = trace_event->thread_timestamp();
    }
  }

  // TODO(eseckler): Split comma-separated category strings.
  track_event->add_category_iids(interned_category->id);

  if (interned_source_location) {
    track_event->set_task_execution()->set_posted_from_iid(
        interned_source_location->id);
  } else if (!privacy_filtering_enabled_) {
    WriteDebugAnnotations(trace_event, track_event, interned_annotation_names);
  }

  auto* legacy_event = track_event->set_legacy_event();

  legacy_event->set_name_iid(interned_name->id);

  char phase = trace_event->phase();
  legacy_event->set_phase(phase);

  if (phase == TRACE_EVENT_PHASE_COMPLETE) {
    legacy_event->set_duration_us(trace_event->duration().InMicroseconds());

    if (!trace_event->thread_timestamp().is_null()) {
      int64_t thread_duration = trace_event->thread_duration().InMicroseconds();
      if (thread_duration != -1) {
        legacy_event->set_thread_duration_us(thread_duration);
      }
    }
  } else if (phase == TRACE_EVENT_PHASE_INSTANT) {
    switch (flags & TRACE_EVENT_FLAG_SCOPE_MASK) {
      case TRACE_EVENT_SCOPE_GLOBAL:
        legacy_event->set_instant_event_scope(
            TrackEvent::LegacyEvent::SCOPE_GLOBAL);
        break;

      case TRACE_EVENT_SCOPE_PROCESS:
        legacy_event->set_instant_event_scope(
            TrackEvent::LegacyEvent::SCOPE_PROCESS);
        break;

      case TRACE_EVENT_SCOPE_THREAD:
        legacy_event->set_instant_event_scope(
            TrackEvent::LegacyEvent::SCOPE_THREAD);
        break;
    }
  }

  uint32_t id_flags =
      flags & (TRACE_EVENT_FLAG_HAS_ID | TRACE_EVENT_FLAG_HAS_LOCAL_ID |
               TRACE_EVENT_FLAG_HAS_GLOBAL_ID);
  switch (id_flags) {
    case TRACE_EVENT_FLAG_HAS_ID:
      legacy_event->set_unscoped_id(trace_event->id());
      break;
    case TRACE_EVENT_FLAG_HAS_LOCAL_ID:
      legacy_event->set_local_id(trace_event->id());
      break;
    case TRACE_EVENT_FLAG_HAS_GLOBAL_ID:
      legacy_event->set_global_id(trace_event->id());
      break;
    default:
      break;
  }

  // TODO(ssid): Add scope field as enum and do not filter this field.
  if (!privacy_filtering_enabled_) {
    if (id_flags &&
        trace_event->scope() != trace_event_internal::kGlobalScope) {
      legacy_event->set_id_scope(trace_event->scope());
    }
  }

  if (flags & TRACE_EVENT_FLAG_ASYNC_TTS) {
    legacy_event->set_use_async_tts(true);
  }

  uint32_t flow_flags =
      flags & (TRACE_EVENT_FLAG_FLOW_OUT | TRACE_EVENT_FLAG_FLOW_IN);
  switch (flow_flags) {
    case TRACE_EVENT_FLAG_FLOW_OUT | TRACE_EVENT_FLAG_FLOW_IN:
      legacy_event->set_flow_direction(TrackEvent::LegacyEvent::FLOW_INOUT);
      break;
    case TRACE_EVENT_FLAG_FLOW_OUT:
      legacy_event->set_flow_direction(TrackEvent::LegacyEvent::FLOW_OUT);
      break;
    case TRACE_EVENT_FLAG_FLOW_IN:
      legacy_event->set_flow_direction(TrackEvent::LegacyEvent::FLOW_IN);
      break;
    default:
      break;
  }

  if (flow_flags) {
    legacy_event->set_bind_id(trace_event->bind_id());
  }

  if (flags & TRACE_EVENT_FLAG_BIND_TO_ENCLOSING) {
    legacy_event->set_bind_to_enclosing(true);
  }

  if ((flags & TRACE_EVENT_FLAG_HAS_PROCESS_ID) &&
      trace_event->process_id() != base::kNullProcessId) {
    legacy_event->set_pid_override(trace_event->process_id());
    legacy_event->set_tid_override(-1);
  } else if (thread_id_ != trace_event->thread_id()) {
    legacy_event->set_tid_override(trace_event->thread_id());
  }

  // Emit any new interned data entries into the packet.
  auto* interned_data = trace_packet->set_interned_data();
  if (!interned_category->was_emitted) {
    auto* category_entry = interned_data->add_event_categories();
    category_entry->set_iid(interned_category->id);
    category_entry->set_name(category_name);
    interned_category->was_emitted = true;
  }

  if (!interned_name->was_emitted) {
    auto* name_entry = interned_data->add_legacy_event_names();
    name_entry->set_iid(interned_name->id);
    name_entry->set_name(copy_strings && privacy_filtering_enabled_
                             ? kPrivacyFiltered
                             : trace_event->name());
    interned_name->was_emitted = true;
  }

  if (interned_source_location) {
    if (!interned_source_location->was_emitted) {
      auto* source_location_entry = interned_data->add_source_locations();
      source_location_entry->set_iid(interned_source_location->id);
      source_location_entry->set_file_name(trace_event->arg_value(0).as_string);
      if (trace_event->arg_size() > 1) {
        source_location_entry->set_function_name(
            trace_event->arg_value(1).as_string);
      }
      interned_source_location->was_emitted = true;
    }
  } else if (!privacy_filtering_enabled_) {
    for (size_t i = 0; i < trace_event->arg_size() && trace_event->arg_name(i);
         ++i) {
      DCHECK(interned_annotation_names[i]);
      if (!interned_annotation_names[i]->was_emitted) {
        auto* name_entry = interned_data->add_debug_annotation_names();
        name_entry->set_iid(interned_annotation_names[i]->id);
        name_entry->set_name(trace_event->arg_name(i));
        interned_annotation_names[i]->was_emitted = true;
      }
    }
  }

  if (disable_interning_) {
    interned_event_categories_.Clear();
    interned_event_names_.Clear();
    interned_annotation_names_.Clear();
    interned_source_locations_.Clear();
  }

  events_since_last_incremental_state_reset_++;
  if (events_since_last_incremental_state_reset_ >=
      kMaxEventsBeforeIncrementalStateReset) {
    ResetIncrementalState();
  }
}

void TrackEventThreadLocalEventSink::UpdateDuration(
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

void TrackEventThreadLocalEventSink::Flush() {
  trace_writer_->Flush();
}

}  // namespace tracing

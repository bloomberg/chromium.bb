// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/tracing/perfetto/track_event_json_exporter.h"

#include <cinttypes>

#include "base/json/string_escape.h"
#include "base/strings/string_util.h"
#include "base/trace_event/common/trace_event_common.h"
#include "third_party/perfetto/protos/perfetto/trace/chrome/chrome_trace_packet.pb.h"

namespace tracing {
namespace {

using ::perfetto::protos::ChromeTracePacket;
using ::perfetto::protos::TrackEvent;

const std::string& GetInternedName(
    uint32_t iid,
    const std::unordered_map<uint32_t, std::string>& interned) {
  auto iter = interned.find(iid);
  DCHECK(iter != interned.end());
  return iter->second;
}

}  // namespace

TrackEventJSONExporter::TrackEventJSONExporter(
    JSONTraceExporter::ArgumentFilterPredicate argument_filter_predicate,
    JSONTraceExporter::MetadataFilterPredicate metadata_filter_predicate,
    JSONTraceExporter::OnTraceEventJSONCallback callback)
    : JSONTraceExporter(std::move(argument_filter_predicate),
                        std::move(metadata_filter_predicate),
                        std::move(callback)),
      current_state_(0) {}

TrackEventJSONExporter::~TrackEventJSONExporter() {}

void TrackEventJSONExporter::ProcessPackets(
    const std::vector<perfetto::TracePacket>& packets) {
  for (auto& encoded_packet : packets) {
    // These are perfetto::TracePackets, but ChromeTracePacket is a mirror that
    // reduces binary bloat and only has the fields we are interested in. So
    // Decode the serialized proto as a ChromeTracePacket.
    perfetto::protos::ChromeTracePacket packet;
    bool decoded = encoded_packet.Decode(&packet);
    DCHECK(decoded);

    // If this is a different packet_sequence_id we have to reset all our state
    // and wait for the first state_clear before emitting anything.
    if (packet.trusted_packet_sequence_id() !=
        current_state_.trusted_packet_sequence_id) {
      StartNewState(packet.trusted_packet_sequence_id(),
                    packet.incremental_state_cleared());
    } else if (packet.incremental_state_cleared()) {
      ResetIncrementalState();
    } else if (packet.previous_packet_dropped()) {
      // If we've lost packets we can no longer trust any timestamp data and
      // other state which might have been dropped. We will keep skipping events
      // until we start a new sequence.
      LOG_IF(ERROR, current_state_.incomplete)
          << "Previous packet was dropped. Skipping TraceEvents until reset or "
          << "new sequence.";
      current_state_.incomplete = true;
    }

    // Now we process the data from the packet. First by getting the interned
    // strings out and processed.
    if (packet.has_interned_data()) {
      HandleInternedData(packet);
    }

    // These are all oneof fields below so only one should be true.
    if (packet.has_track_event()) {
      HandleTrackEvent(packet);
    } else if (packet.has_chrome_events()) {
      HandleChromeEvents(packet);
    } else if (packet.has_thread_descriptor()) {
      HandleThreadDescriptor(packet);
    } else if (packet.has_process_descriptor()) {
      HandleProcessDescriptor(packet);
    } else if (packet.has_trace_stats()) {
      SetTraceStatsMetadata(packet.trace_stats());
    } else {
      // If none of the above matched, this packet was emitted by the service
      // and has no equivalent in the old trace format. We thus ignore it.
    }
  }
}

TrackEventJSONExporter::ProducerWriterState::ProducerWriterState(
    uint32_t sequence_id)
    : ProducerWriterState(sequence_id, false, false, true) {}

TrackEventJSONExporter::ProducerWriterState::ProducerWriterState(
    uint32_t sequence_id,
    bool emitted_process,
    bool emitted_thread,
    bool incomplete)
    : trusted_packet_sequence_id(sequence_id),
      emitted_process_metadata(emitted_process),
      emitted_thread_metadata(emitted_thread),
      incomplete(incomplete) {}

TrackEventJSONExporter::ProducerWriterState::~ProducerWriterState() {}

void TrackEventJSONExporter::StartNewState(uint32_t trusted_packet_sequence_id,
                                           bool state_cleared) {
  current_state_ = ProducerWriterState{
      trusted_packet_sequence_id, /* emitted_process = */ false,
      /* emitted_thread = */ false, /* incomplete = */ !state_cleared};
}

void TrackEventJSONExporter::ResetIncrementalState() {
  current_state_ =
      ProducerWriterState{current_state_.trusted_packet_sequence_id,
                          current_state_.emitted_process_metadata,
                          current_state_.emitted_thread_metadata,
                          /* incomplete = */ false};
}

int64_t TrackEventJSONExporter::ComputeTimeUs(const TrackEvent& event) {
  switch (event.timestamp_case()) {
    case TrackEvent::kTimestampAbsoluteUs:
      return event.timestamp_absolute_us();
    case TrackEvent::kTimestampDeltaUs:
      DCHECK(current_state_.time_us != -1);
      current_state_.time_us += event.timestamp_delta_us();
      return current_state_.time_us;
    case TrackEvent::TIMESTAMP_NOT_SET:
      DLOG(FATAL) << "Event has no timestamp this shouldn't be possible";
      return -1;
  }
}

base::Optional<int64_t> TrackEventJSONExporter::ComputeThreadTimeUs(
    const TrackEvent& event) {
  switch (event.thread_time_case()) {
    case TrackEvent::kThreadTimeAbsoluteUs:
      return event.thread_time_absolute_us();
    case TrackEvent::kThreadTimeDeltaUs:
      DCHECK(current_state_.thread_time_us != -1);
      current_state_.thread_time_us += event.thread_time_delta_us();
      return current_state_.thread_time_us;
    case TrackEvent::THREAD_TIME_NOT_SET:
      return base::nullopt;
  }
}

void TrackEventJSONExporter::HandleInternedData(
    const ChromeTracePacket& packet) {
  DCHECK(packet.has_interned_data());

  // InternedData is only emitted on sequences with incremental state.
  if (current_state_.incomplete) {
    return;
  }

  const auto& data = packet.interned_data();
  // Even if the interned data was reset we should not change the values in the
  // interned data.
  for (const auto& event_cat : data.event_categories()) {
    auto iter = current_state_.interned_event_categories_.insert(
        std::make_pair(event_cat.iid(), event_cat.name()));
    DCHECK(iter.second || iter.first->second == event_cat.name());
  }
  for (const auto& event_name : data.legacy_event_names()) {
    auto iter = current_state_.interned_legacy_event_names_.insert(
        std::make_pair(event_name.iid(), event_name.name()));
    DCHECK(iter.second || iter.first->second == event_name.name());
  }
  for (const auto& debug_name : data.debug_annotation_names()) {
    auto iter = current_state_.interned_debug_annotation_names_.insert(
        std::make_pair(debug_name.iid(), debug_name.name()));
    DCHECK(iter.second || iter.first->second == debug_name.name());
  }
  for (const auto& src_loc : data.source_locations()) {
    auto iter = current_state_.interned_source_locations_.insert(std::make_pair(
        src_loc.iid(),
        std::make_pair(src_loc.file_name(), src_loc.function_name())));
    DCHECK(iter.second ||
           (iter.first->second.first == src_loc.file_name() &&
            iter.first->second.second == src_loc.function_name()));
  }
}

void TrackEventJSONExporter::HandleProcessDescriptor(
    const ChromeTracePacket& packet) {
  DCHECK(packet.has_process_descriptor());
  const auto& process = packet.process_descriptor();
  // Save the current state we need for future packets.
  current_state_.pid = process.pid();

  // ProcessDescriptor is only emitted on sequences with incremental state.
  if (current_state_.incomplete) {
    return;
  }

  // If we aren't outputting traceEvents then we don't need to look at the
  // metadata that might need to be emitted.
  if (!ShouldOutputTraceEvents()) {
    return;
  }

  // Prevent duplicates by only emitting the metadata once.
  if (current_state_.emitted_process_metadata) {
    return;
  }
  current_state_.emitted_process_metadata = true;

  if (!process.cmdline().empty()) {
    NOTIMPLEMENTED();
  }

  if (process.has_legacy_sort_index()) {
    auto event_builder =
        AddTraceEvent("process_sort_index", "__metadata", 'M', 0,
                      current_state_.pid, current_state_.pid);
    auto args_builder = event_builder.BuildArgs();
    auto* add_arg = args_builder->MaybeAddArg("sort_index");
    if (add_arg) {
      add_arg->AppendF("%" PRId32, process.legacy_sort_index());
    }
  }

  const auto emit_process_name = [this](const char* name) {
    auto event_builder = AddTraceEvent("process_name", "__metadata", 'M', 0,
                                       current_state_.pid, current_state_.pid);
    auto args_builder = event_builder.BuildArgs();
    auto* add_arg = args_builder->MaybeAddArg("name");
    if (add_arg) {
      add_arg->AppendF("\"%s\"", name);
    }
  };
  switch (process.chrome_process_type()) {
    case perfetto::protos::ProcessDescriptor::PROCESS_UNSPECIFIED:
      // This process does not have a name.
      break;
    case perfetto::protos::ProcessDescriptor::PROCESS_BROWSER:
      emit_process_name("BROWSER");
      break;
    case perfetto::protos::ProcessDescriptor::PROCESS_RENDERER:
      emit_process_name("RENDERER");
      break;
    case perfetto::protos::ProcessDescriptor::PROCESS_UTILITY:
      emit_process_name("UTILITY");
      break;
    case perfetto::protos::ProcessDescriptor::PROCESS_ZYGOTE:
      emit_process_name("ZYGOTE");
      break;
    case perfetto::protos::ProcessDescriptor::PROCESS_SANDBOX_HELPER:
      emit_process_name("SANDBOX_HELPER");
      break;
    case perfetto::protos::ProcessDescriptor::PROCESS_GPU:
      emit_process_name("GPU");
      break;
    case perfetto::protos::ProcessDescriptor::PROCESS_PPAPI_PLUGIN:
      emit_process_name("PPAPI_PLUGIN");
      break;
    case perfetto::protos::ProcessDescriptor::PROCESS_PPAPI_BROKER:
      emit_process_name("PPAPI_BROKER");
      break;
  }
}

void TrackEventJSONExporter::HandleThreadDescriptor(
    const ChromeTracePacket& packet) {
  DCHECK(packet.has_thread_descriptor());

  // ThreadDescriptor is only emitted on sequences with incremental state.
  if (current_state_.incomplete) {
    return;
  }

  const auto& thread = packet.thread_descriptor();
  // Save the current state we need for future packets.
  current_state_.pid = thread.pid();
  current_state_.tid = thread.tid();
  current_state_.time_us = thread.reference_timestamp_us();
  current_state_.thread_time_us = thread.reference_thread_time_us();

  // If we aren't outputting traceEvents then we don't need to look at the
  // metadata that might need to be emitted.
  if (!ShouldOutputTraceEvents()) {
    return;
  }

  // Prevent duplicates by only emitting the metadata once.
  if (current_state_.emitted_thread_metadata) {
    return;
  }
  current_state_.emitted_thread_metadata = true;

  if (thread.has_legacy_sort_index()) {
    auto event_builder =
        AddTraceEvent("thread_sort_index", "__metadata", 'M', 0,
                      current_state_.pid, current_state_.tid);
    auto args_builder = event_builder.BuildArgs();
    auto* add_arg = args_builder->MaybeAddArg("sort_index");
    if (add_arg) {
      add_arg->AppendF("%" PRId32, thread.legacy_sort_index());
    }
  }

  const auto emit_thread_name = [this](const char* name) {
    auto event_builder = AddTraceEvent("thread_name", "__metadata", 'M', 0,
                                       current_state_.pid, current_state_.tid);
    auto args_builder = event_builder.BuildArgs();
    auto* add_arg = args_builder->MaybeAddArg("name");
    if (add_arg) {
      add_arg->AppendF("\"%s\"", name);
    }
  };
  switch (thread.chrome_thread_type()) {
    // TODO(nuskos): As we add more thread types we will add handling here to
    // switch the enum to a string and call |emit_thread_name()|
    case perfetto::protos::ThreadDescriptor::THREAD_UNSPECIFIED:
      // No thread type enum so check to see if a explicit thread name was
      // provided..
      if (thread.has_thread_name()) {
        emit_thread_name(thread.thread_name().c_str());
      }
      break;
  }
}

void TrackEventJSONExporter::HandleChromeEvents(
    const perfetto::protos::ChromeTracePacket& packet) {
  DCHECK(packet.has_chrome_events());

  const auto& chrome_events = packet.chrome_events();
  DCHECK(chrome_events.trace_events().empty())
      << "Found trace_events inside a ChromeEventBundle. This shouldn't "
      << "happen when emitting TrackEvents.";

  for (const auto& metadata : chrome_events.metadata()) {
    AddChromeMetadata(metadata);
  }
  for (const auto& legacy_ftrace : chrome_events.legacy_ftrace_output()) {
    AddLegacyFtrace(legacy_ftrace);
  }
  for (const auto& legacy_json_trace : chrome_events.legacy_json_trace()) {
    AddChromeLegacyJSONTrace(legacy_json_trace);
  }
}

void TrackEventJSONExporter::HandleTrackEvent(const ChromeTracePacket& packet) {
  DCHECK(packet.has_track_event());

  // TrackEvents need incremental state.
  if (current_state_.incomplete) {
    return;
  }

  // If we aren't outputting traceEvents nothing in a TrackEvent currently will
  // be needed so just return early.
  if (!ShouldOutputTraceEvents()) {
    return;
  }

  const auto& track = packet.track_event();

  // Get the time data out of the TrackEvent.
  int64_t timestamp_us = ComputeTimeUs(track);
  DCHECK(timestamp_us != -1);
  base::Optional<int64_t> thread_time_us = ComputeThreadTimeUs(track);

  std::vector<base::StringPiece> all_categories;
  all_categories.reserve(track.category_iids().size());
  for (const auto& cat_iid : track.category_iids()) {
    const std::string& name =
        GetInternedName(cat_iid, current_state_.interned_event_categories_);
    all_categories.push_back(name);
  }
  const std::string joined_categories = base::JoinString(all_categories, ",");

  DCHECK(track.has_legacy_event()) << "required field legacy_event missing";
  auto maybe_builder =
      HandleLegacyEvent(track.legacy_event(), joined_categories, timestamp_us);
  if (!maybe_builder) {
    return;
  }
  auto& builder = *maybe_builder;

  if (thread_time_us) {
    builder.AddThreadTimestamp(*thread_time_us);
  }

  // Now we add args from both |task_execution| and |debug_annotations|. Recall
  // that |args_builder| must run its deconstructer before any other fields in
  // traceEvents are added. Therefore do not do anything below this comment but
  // add args.
  auto args_builder = builder.BuildArgs();

  for (const auto& debug_annotation : track.debug_annotations()) {
    HandleDebugAnnotation(debug_annotation, args_builder.get());
  }

  if (track.has_task_execution()) {
    HandleTaskExecution(track.task_execution(), args_builder.get());
  }
}

void TrackEventJSONExporter::HandleDebugAnnotation(
    const perfetto::protos::DebugAnnotation& debug_annotation,
    ArgumentBuilder* args_builder) {
  const std::string& name =
      GetInternedName(debug_annotation.name_iid(),
                      current_state_.interned_debug_annotation_names_);

  auto* maybe_arg = args_builder->MaybeAddArg(name);
  if (!maybe_arg) {
    return;
  }
  OutputJSONFromArgumentProto(debug_annotation, maybe_arg->mutable_out());
}

void TrackEventJSONExporter::HandleTaskExecution(
    const perfetto::protos::TaskExecution& task,
    ArgumentBuilder* args_builder) {
  auto iter =
      current_state_.interned_source_locations_.find(task.posted_from_iid());
  DCHECK(iter != current_state_.interned_source_locations_.end());

  // If source locations were turned off, only the file is provided. JSON
  // expects the event to then have only an "src" attribute.
  if (iter->second.second.empty()) {
    auto* maybe_arg = args_builder->MaybeAddArg("src");
    if (maybe_arg) {
      base::EscapeJSONString(iter->second.first, true,
                             maybe_arg->mutable_out());
    }
    return;
  }

  auto* maybe_arg = args_builder->MaybeAddArg("src_file");
  if (maybe_arg) {
    base::EscapeJSONString(iter->second.first, true, maybe_arg->mutable_out());
  }
  maybe_arg = args_builder->MaybeAddArg("src_func");
  if (maybe_arg) {
    base::EscapeJSONString(iter->second.second, true, maybe_arg->mutable_out());
  }
}

base::Optional<JSONTraceExporter::ScopedJSONTraceEventAppender>
TrackEventJSONExporter::HandleLegacyEvent(const TrackEvent::LegacyEvent& event,
                                          const std::string& categories,
                                          int64_t timestamp_us) {
  DCHECK(event.name_iid());
  DCHECK(event.phase());

  // Determine which pid and tid to use.
  int32_t pid =
      event.pid_override() == 0 ? current_state_.pid : event.pid_override();
  int32_t tid =
      event.tid_override() == 0 ? current_state_.tid : event.tid_override();

  const std::string& name = GetInternedName(
      event.name_iid(), current_state_.interned_legacy_event_names_);

  // Build the actual json output, if we are missing the interned name we just
  // use the interned ID.
  auto builder = AddTraceEvent(name.c_str(), categories.c_str(), event.phase(),
                               timestamp_us, pid, tid);

  if (event.has_bind_id()) {
    builder.AddBindId(event.bind_id());
  }
  if (event.has_duration_us()) {
    builder.AddDuration(event.duration_us());
  }
  if (event.has_thread_duration_us()) {
    builder.AddThreadDuration(event.thread_duration_us());
  }

  // For flags and ID we need to determine all possible flag bits and set them
  // correctly.
  uint32_t flags = 0;
  base::Optional<uint32_t> id;
  switch (event.id_case()) {
    case TrackEvent::LegacyEvent::kUnscopedId:
      flags |= TRACE_EVENT_FLAG_HAS_ID;
      id = event.unscoped_id();
      break;
    case TrackEvent::LegacyEvent::kLocalId:
      flags |= TRACE_EVENT_FLAG_HAS_LOCAL_ID;
      id = event.local_id();
      break;
    case TrackEvent::LegacyEvent::kGlobalId:
      flags |= TRACE_EVENT_FLAG_HAS_GLOBAL_ID;
      id = event.global_id();
      break;
    case TrackEvent::LegacyEvent::ID_NOT_SET:
      break;
  }
  if (event.use_async_tts()) {
    flags |= TRACE_EVENT_FLAG_ASYNC_TTS;
  }
  if (event.bind_to_enclosing()) {
    flags |= TRACE_EVENT_FLAG_BIND_TO_ENCLOSING;
  }
  switch (event.flow_direction()) {
    case TrackEvent::LegacyEvent::FLOW_IN:
      flags |= TRACE_EVENT_FLAG_FLOW_IN;
      break;
    case TrackEvent::LegacyEvent::FLOW_OUT:
      flags |= TRACE_EVENT_FLAG_FLOW_OUT;
      break;
    case TrackEvent::LegacyEvent::FLOW_INOUT:
      flags |= TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT;
      break;
    case TrackEvent::LegacyEvent::FLOW_UNSPECIFIED:
      break;
  }
  switch (event.instant_event_scope()) {
    case TrackEvent::LegacyEvent::SCOPE_GLOBAL:
      flags |= TRACE_EVENT_SCOPE_GLOBAL;
      break;
    case TrackEvent::LegacyEvent::SCOPE_PROCESS:
      flags |= TRACE_EVENT_SCOPE_PROCESS;
      break;
    case TrackEvent::LegacyEvent::SCOPE_THREAD:
      flags |= TRACE_EVENT_SCOPE_THREAD;
      break;
    case TrackEvent::LegacyEvent::SCOPE_UNSPECIFIED:
      break;
  }
  // Even if |flags==0|, we need to call AddFlags to output instant event scope.
  builder.AddFlags(flags, id, event.id_scope());
  return builder;
}
}  // namespace tracing

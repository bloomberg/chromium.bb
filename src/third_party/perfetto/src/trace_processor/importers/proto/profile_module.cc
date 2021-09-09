/*
 * Copyright (C) 2020 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "src/trace_processor/importers/proto/profile_module.h"

#include "perfetto/base/logging.h"
#include "src/trace_processor/importers/common/clock_tracker.h"
#include "src/trace_processor/importers/common/event_tracker.h"
#include "src/trace_processor/importers/common/process_tracker.h"
#include "src/trace_processor/importers/proto/packet_sequence_state.h"
#include "src/trace_processor/importers/proto/perf_sample_tracker.h"
#include "src/trace_processor/importers/proto/profile_packet_utils.h"
#include "src/trace_processor/importers/proto/stack_profile_tracker.h"
#include "src/trace_processor/storage/trace_storage.h"
#include "src/trace_processor/tables/profiler_tables.h"
#include "src/trace_processor/timestamped_trace_piece.h"
#include "src/trace_processor/trace_sorter.h"
#include "src/trace_processor/types/trace_processor_context.h"

#include "protos/perfetto/common/builtin_clock.pbzero.h"
#include "protos/perfetto/common/perf_events.pbzero.h"
#include "protos/perfetto/trace/profiling/profile_packet.pbzero.h"

namespace perfetto {
namespace trace_processor {

using perfetto::protos::pbzero::TracePacket;
using protozero::ConstBytes;

ProfileModule::ProfileModule(TraceProcessorContext* context)
    : context_(context) {
  RegisterForField(TracePacket::kStreamingProfilePacketFieldNumber, context);
  RegisterForField(TracePacket::kPerfSampleFieldNumber, context);
}

ProfileModule::~ProfileModule() = default;

ModuleResult ProfileModule::TokenizePacket(const TracePacket::Decoder& decoder,
                                           TraceBlobView* packet,
                                           int64_t /*packet_timestamp*/,
                                           PacketSequenceState* state,
                                           uint32_t field_id) {
  switch (field_id) {
    case TracePacket::kStreamingProfilePacketFieldNumber:
      return TokenizeStreamingProfilePacket(state, packet,
                                            decoder.streaming_profile_packet());
  }
  return ModuleResult::Ignored();
}

void ProfileModule::ParsePacket(const TracePacket::Decoder& decoder,
                                const TimestampedTracePiece& ttp,
                                uint32_t field_id) {
  switch (field_id) {
    case TracePacket::kStreamingProfilePacketFieldNumber:
      PERFETTO_DCHECK(ttp.type == TimestampedTracePiece::Type::kTracePacket);
      ParseStreamingProfilePacket(ttp.timestamp,
                                  ttp.packet_data.sequence_state.get(),
                                  decoder.streaming_profile_packet());
      return;
    case TracePacket::kPerfSampleFieldNumber:
      PERFETTO_DCHECK(ttp.type == TimestampedTracePiece::Type::kTracePacket);
      ParsePerfSample(ttp.timestamp, ttp.packet_data.sequence_state.get(),
                      decoder);
      return;
  }
}

ModuleResult ProfileModule::TokenizeStreamingProfilePacket(
    PacketSequenceState* sequence_state,
    TraceBlobView* packet,
    ConstBytes streaming_profile_packet) {
  protos::pbzero::StreamingProfilePacket::Decoder decoder(
      streaming_profile_packet.data, streaming_profile_packet.size);

  // We have to resolve the reference timestamp of a StreamingProfilePacket
  // during tokenization. If we did this during parsing instead, the
  // tokenization of a subsequent ThreadDescriptor with a new reference
  // timestamp would cause us to later calculate timestamps based on the wrong
  // reference value during parsing. Since StreamingProfilePackets only need to
  // be sorted correctly with respect to process/thread metadata events (so that
  // pid/tid are resolved correctly during parsing), we forward the packet as a
  // whole through the sorter, using the "root" timestamp of the packet, i.e.
  // the current timestamp of the packet sequence.
  auto packet_ts =
      sequence_state->IncrementAndGetTrackEventTimeNs(/*delta_ns=*/0);
  auto trace_ts = context_->clock_tracker->ToTraceTime(
      protos::pbzero::BUILTIN_CLOCK_MONOTONIC, packet_ts);
  if (trace_ts)
    packet_ts = *trace_ts;

  // Increment the sequence's timestamp by all deltas.
  for (auto timestamp_it = decoder.timestamp_delta_us(); timestamp_it;
       ++timestamp_it) {
    sequence_state->IncrementAndGetTrackEventTimeNs(*timestamp_it * 1000);
  }

  context_->sorter->PushTracePacket(packet_ts, sequence_state,
                                    std::move(*packet));
  return ModuleResult::Handled();
}

void ProfileModule::ParseStreamingProfilePacket(
    int64_t timestamp,
    PacketSequenceStateGeneration* sequence_state,
    ConstBytes streaming_profile_packet) {
  protos::pbzero::StreamingProfilePacket::Decoder packet(
      streaming_profile_packet.data, streaming_profile_packet.size);

  ProcessTracker* procs = context_->process_tracker.get();
  TraceStorage* storage = context_->storage.get();
  SequenceStackProfileTracker& sequence_stack_profile_tracker =
      sequence_state->state()->sequence_stack_profile_tracker();
  ProfilePacketInternLookup intern_lookup(sequence_state);

  uint32_t pid = static_cast<uint32_t>(sequence_state->state()->pid());
  uint32_t tid = static_cast<uint32_t>(sequence_state->state()->tid());
  UniqueTid utid = procs->UpdateThread(tid, pid);

  // Iterate through timestamps and callstacks simultaneously.
  auto timestamp_it = packet.timestamp_delta_us();
  for (auto callstack_it = packet.callstack_iid(); callstack_it;
       ++callstack_it, ++timestamp_it) {
    if (!timestamp_it) {
      context_->storage->IncrementStats(stats::stackprofile_parser_error);
      PERFETTO_ELOG(
          "StreamingProfilePacket has less callstack IDs than timestamps!");
      break;
    }

    auto opt_cs_id = sequence_stack_profile_tracker.FindOrInsertCallstack(
        *callstack_it, &intern_lookup);
    if (!opt_cs_id) {
      context_->storage->IncrementStats(stats::stackprofile_parser_error);
      continue;
    }

    // Resolve the delta timestamps based on the packet's root timestamp.
    timestamp += *timestamp_it * 1000;

    tables::CpuProfileStackSampleTable::Row sample_row{
        timestamp, *opt_cs_id, utid, packet.process_priority()};
    storage->mutable_cpu_profile_stack_sample_table()->Insert(sample_row);
  }
}

void ProfileModule::ParsePerfSample(
    int64_t ts,
    PacketSequenceStateGeneration* sequence_state,
    const TracePacket::Decoder& decoder) {
  using PerfSample = protos::pbzero::PerfSample;
  const auto& sample_raw = decoder.perf_sample();
  PerfSample::Decoder sample(sample_raw.data, sample_raw.size);

  uint32_t seq_id = decoder.trusted_packet_sequence_id();
  PerfSampleTracker::SamplingStreamInfo sampling_stream =
      context_->perf_sample_tracker->GetSamplingStreamInfo(
          seq_id, sample.cpu(), sequence_state->GetTracePacketDefaults());

  // Not a sample, but an indication of data loss in the ring buffer shared with
  // the kernel.
  if (sample.kernel_records_lost() > 0) {
    PERFETTO_DCHECK(sample.pid() == 0);

    context_->storage->IncrementIndexedStats(
        stats::perf_cpu_lost_records, static_cast<int>(sample.cpu()),
        static_cast<int64_t>(sample.kernel_records_lost()));
    return;
  }

  // Sample that looked relevant for the tracing session, but had to be skipped.
  // Either we failed to look up the procfs file descriptors necessary for
  // remote stack unwinding (not unexpected in most cases), or the unwind queue
  // was out of capacity (producer lost data on its own).
  if (sample.has_sample_skipped_reason()) {
    context_->storage->IncrementStats(stats::perf_samples_skipped);

    if (sample.sample_skipped_reason() ==
        PerfSample::PROFILER_SKIP_UNWIND_ENQUEUE)
      context_->storage->IncrementStats(stats::perf_samples_skipped_dataloss);

    return;
  }

  // Not a sample, but an event from the producer.
  // TODO(rsavitski): this stat is indexed by the session id, but the older
  // stats (see above) aren't. The indexing is relevant if a trace contains more
  // than one profiling data source. So the older stats should be changed to
  // being indexed as well.
  if (sample.has_producer_event()) {
    PerfSample::ProducerEvent::Decoder producer_event(sample.producer_event());
    if (producer_event.source_stop_reason() ==
        PerfSample::ProducerEvent::PROFILER_STOP_GUARDRAIL) {
      context_->storage->SetIndexedStats(
          stats::perf_guardrail_stop_ts,
          static_cast<int>(sampling_stream.perf_session_id), ts);
    }
    return;
  }

  // Proper sample, populate the |perf_sample| table with everything except the
  // recorded counter values, which go to |counter|.
  context_->event_tracker->PushCounter(
      ts, static_cast<double>(sample.timebase_count()),
      sampling_stream.timebase_track_id);

  // TODO(rsavitski): empty callsite is not an error for counter-only samples.
  // But consider identifying sequences which *should* have a callstack in every
  // sample, as an invalid stack there is a bug.
  SequenceStackProfileTracker& stack_tracker =
      sequence_state->state()->sequence_stack_profile_tracker();
  ProfilePacketInternLookup intern_lookup(sequence_state);
  uint64_t callstack_iid = sample.callstack_iid();
  base::Optional<CallsiteId> cs_id =
      stack_tracker.FindOrInsertCallstack(callstack_iid, &intern_lookup);

  UniqueTid utid =
      context_->process_tracker->UpdateThread(sample.tid(), sample.pid());

  using protos::pbzero::Profiling;
  TraceStorage* storage = context_->storage.get();

  auto cpu_mode = static_cast<Profiling::CpuMode>(sample.cpu_mode());
  StringPool::Id cpu_mode_id =
      storage->InternString(ProfilePacketUtils::StringifyCpuMode(cpu_mode));

  base::Optional<StringPool::Id> unwind_error_id;
  if (sample.has_unwind_error()) {
    auto unwind_error =
        static_cast<Profiling::StackUnwindError>(sample.unwind_error());
    unwind_error_id = storage->InternString(
        ProfilePacketUtils::StringifyStackUnwindError(unwind_error));
  }
  tables::PerfSampleTable::Row sample_row(ts, utid, sample.cpu(), cpu_mode_id,
                                          cs_id, unwind_error_id,
                                          sampling_stream.perf_session_id);
  context_->storage->mutable_perf_sample_table()->Insert(sample_row);
}

}  // namespace trace_processor
}  // namespace perfetto

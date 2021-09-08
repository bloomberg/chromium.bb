/*
 * Copyright (C) 2018 The Android Open Source Project
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

#include "src/trace_processor/importers/proto/proto_trace_parser.h"

#include <inttypes.h>
#include <string.h>

#include <string>

#include "perfetto/base/logging.h"
#include "perfetto/ext/base/metatrace_events.h"
#include "perfetto/ext/base/string_utils.h"
#include "perfetto/ext/base/string_view.h"
#include "perfetto/ext/base/string_writer.h"
#include "perfetto/ext/base/utils.h"
#include "perfetto/ext/base/uuid.h"
#include "perfetto/trace_processor/status.h"

#include "src/trace_processor/importers/common/args_tracker.h"
#include "src/trace_processor/importers/common/clock_tracker.h"
#include "src/trace_processor/importers/common/event_tracker.h"
#include "src/trace_processor/importers/common/process_tracker.h"
#include "src/trace_processor/importers/common/slice_tracker.h"
#include "src/trace_processor/importers/common/track_tracker.h"
#include "src/trace_processor/importers/config.descriptor.h"
#include "src/trace_processor/importers/ftrace/ftrace_module.h"
#include "src/trace_processor/importers/proto/heap_profile_tracker.h"
#include "src/trace_processor/importers/proto/metadata_tracker.h"
#include "src/trace_processor/importers/proto/packet_sequence_state.h"
#include "src/trace_processor/importers/proto/profile_packet_utils.h"
#include "src/trace_processor/importers/proto/profiler_util.h"
#include "src/trace_processor/importers/proto/stack_profile_tracker.h"
#include "src/trace_processor/storage/metadata.h"
#include "src/trace_processor/storage/stats.h"
#include "src/trace_processor/tables/profiler_tables.h"
#include "src/trace_processor/timestamped_trace_piece.h"
#include "src/trace_processor/types/trace_processor_context.h"
#include "src/trace_processor/types/variadic.h"
#include "src/trace_processor/util/descriptors.h"
#include "src/trace_processor/util/protozero_to_text.h"

#include "protos/perfetto/common/builtin_clock.pbzero.h"
#include "protos/perfetto/common/trace_stats.pbzero.h"
#include "protos/perfetto/config/trace_config.pbzero.h"
#include "protos/perfetto/trace/chrome/chrome_trace_event.pbzero.h"
#include "protos/perfetto/trace/interned_data/interned_data.pbzero.h"
#include "protos/perfetto/trace/perfetto/perfetto_metatrace.pbzero.h"
#include "protos/perfetto/trace/profiling/deobfuscation.pbzero.h"
#include "protos/perfetto/trace/profiling/profile_common.pbzero.h"
#include "protos/perfetto/trace/profiling/profile_packet.pbzero.h"
#include "protos/perfetto/trace/profiling/smaps.pbzero.h"
#include "protos/perfetto/trace/trace.pbzero.h"
#include "protos/perfetto/trace/trace_packet.pbzero.h"

namespace perfetto {
namespace trace_processor {

ProtoTraceParser::ProtoTraceParser(TraceProcessorContext* context)
    : context_(context),
      metatrace_id_(context->storage->InternString("metatrace")),
      data_name_id_(context->storage->InternString("data")),
      raw_chrome_metadata_event_id_(
          context->storage->InternString("chrome_event.metadata")),
      raw_chrome_legacy_system_trace_event_id_(
          context->storage->InternString("chrome_event.legacy_system_trace")),
      raw_chrome_legacy_user_trace_event_id_(
          context->storage->InternString("chrome_event.legacy_user_trace")) {
  // TODO(140860736): Once we support null values for
  // stack_profile_frame.symbol_set_id remove this hack
  context_->storage->mutable_symbol_table()->Insert(
      {0, kNullStringId, kNullStringId, 0});
}

ProtoTraceParser::~ProtoTraceParser() = default;

void ProtoTraceParser::ParseTracePacket(int64_t ts, TimestampedTracePiece ttp) {
  const TracePacketData* data = nullptr;
  if (ttp.type == TimestampedTracePiece::Type::kTracePacket) {
    data = &ttp.packet_data;
  } else {
    PERFETTO_DCHECK(ttp.type == TimestampedTracePiece::Type::kTrackEvent);
    data = ttp.track_event_data.get();
  }

  const TraceBlobView& blob = data->packet;
  protos::pbzero::TracePacket::Decoder packet(blob.data(), blob.length());

  ParseTracePacketImpl(ts, ttp, data->sequence_state.get(), packet);

  // TODO(lalitm): maybe move this to the flush method in the trace processor
  // once we have it. This may reduce performance in the ArgsTracker though so
  // needs to be handled carefully.
  context_->args_tracker->Flush();
  PERFETTO_DCHECK(!packet.bytes_left());
}

void ProtoTraceParser::ParseTracePacketImpl(
    int64_t ts,
    const TimestampedTracePiece& ttp,
    PacketSequenceStateGeneration* sequence_state,
    const protos::pbzero::TracePacket::Decoder& packet) {
  // This needs to get handled both by the HeapGraphModule and
  // ProtoTraceParser (for StackProfileTracker).
  if (packet.has_deobfuscation_mapping()) {
    ParseDeobfuscationMapping(ts, sequence_state,
                              packet.trusted_packet_sequence_id(),
                              packet.deobfuscation_mapping());
  }

  // Chrome doesn't honor the one-of in TracePacket for this field and sets it
  // together with chrome_metadata, which is handled by a module. Thus, we have
  // to parse this field before the modules get to parse other fields.
  // TODO(crbug/1194914): Move this back after the modules (or into a separate
  // module) once the Chrome-side fix has propagated into all release channels.
  if (packet.has_chrome_events()) {
    ParseChromeEvents(ts, packet.chrome_events());
  }

  // TODO(eseckler): Propagate statuses from modules.
  auto& modules = context_->modules_by_field;
  for (uint32_t field_id = 1; field_id < modules.size(); ++field_id) {
    if (!modules[field_id].empty() && packet.Get(field_id).valid()) {
      for (ProtoImporterModule* module : modules[field_id])
        module->ParsePacket(packet, ttp, field_id);
      return;
    }
  }

  if (packet.has_trace_stats())
    ParseTraceStats(packet.trace_stats());

  if (packet.has_profile_packet()) {
    ParseProfilePacket(ts, sequence_state, packet.trusted_packet_sequence_id(),
                       packet.profile_packet());
  }

  if (packet.has_perfetto_metatrace()) {
    ParseMetatraceEvent(ts, packet.perfetto_metatrace());
  }

  if (packet.has_trace_config()) {
    ParseTraceConfig(packet.trace_config());
  }

  if (packet.has_module_symbols()) {
    ParseModuleSymbols(packet.module_symbols());
  }

  if (packet.has_smaps_packet()) {
    ParseSmapsPacket(ts, packet.smaps_packet());
  }
}

void ProtoTraceParser::ParseFtracePacket(uint32_t cpu,
                                         int64_t /*ts*/,
                                         TimestampedTracePiece ttp) {
  PERFETTO_DCHECK(ttp.type == TimestampedTracePiece::Type::kFtraceEvent ||
                  ttp.type == TimestampedTracePiece::Type::kInlineSchedSwitch ||
                  ttp.type == TimestampedTracePiece::Type::kInlineSchedWaking);
  PERFETTO_DCHECK(context_->ftrace_module);
  context_->ftrace_module->ParseFtracePacket(cpu, ttp);

  // TODO(lalitm): maybe move this to the flush method in the trace processor
  // once we have it. This may reduce performance in the ArgsTracker though so
  // needs to be handled carefully.
  context_->args_tracker->Flush();
}

void ProtoTraceParser::ParseTraceStats(ConstBytes blob) {
  protos::pbzero::TraceStats::Decoder evt(blob.data, blob.size);
  auto* storage = context_->storage.get();
  storage->SetStats(stats::traced_producers_connected,
                    static_cast<int64_t>(evt.producers_connected()));
  storage->SetStats(stats::traced_data_sources_registered,
                    static_cast<int64_t>(evt.data_sources_registered()));
  storage->SetStats(stats::traced_data_sources_seen,
                    static_cast<int64_t>(evt.data_sources_seen()));
  storage->SetStats(stats::traced_tracing_sessions,
                    static_cast<int64_t>(evt.tracing_sessions()));
  storage->SetStats(stats::traced_total_buffers,
                    static_cast<int64_t>(evt.total_buffers()));
  storage->SetStats(stats::traced_chunks_discarded,
                    static_cast<int64_t>(evt.chunks_discarded()));
  storage->SetStats(stats::traced_patches_discarded,
                    static_cast<int64_t>(evt.patches_discarded()));

  int buf_num = 0;
  for (auto it = evt.buffer_stats(); it; ++it, ++buf_num) {
    protos::pbzero::TraceStats::BufferStats::Decoder buf(*it);
    storage->SetIndexedStats(stats::traced_buf_buffer_size, buf_num,
                             static_cast<int64_t>(buf.buffer_size()));
    storage->SetIndexedStats(stats::traced_buf_bytes_written, buf_num,
                             static_cast<int64_t>(buf.bytes_written()));
    storage->SetIndexedStats(stats::traced_buf_bytes_overwritten, buf_num,
                             static_cast<int64_t>(buf.bytes_overwritten()));
    storage->SetIndexedStats(stats::traced_buf_bytes_read, buf_num,
                             static_cast<int64_t>(buf.bytes_read()));
    storage->SetIndexedStats(stats::traced_buf_padding_bytes_written, buf_num,
                             static_cast<int64_t>(buf.padding_bytes_written()));
    storage->SetIndexedStats(stats::traced_buf_padding_bytes_cleared, buf_num,
                             static_cast<int64_t>(buf.padding_bytes_cleared()));
    storage->SetIndexedStats(stats::traced_buf_chunks_written, buf_num,
                             static_cast<int64_t>(buf.chunks_written()));
    storage->SetIndexedStats(stats::traced_buf_chunks_rewritten, buf_num,
                             static_cast<int64_t>(buf.chunks_rewritten()));
    storage->SetIndexedStats(stats::traced_buf_chunks_overwritten, buf_num,
                             static_cast<int64_t>(buf.chunks_overwritten()));
    storage->SetIndexedStats(stats::traced_buf_chunks_discarded, buf_num,
                             static_cast<int64_t>(buf.chunks_discarded()));
    storage->SetIndexedStats(stats::traced_buf_chunks_read, buf_num,
                             static_cast<int64_t>(buf.chunks_read()));
    storage->SetIndexedStats(
        stats::traced_buf_chunks_committed_out_of_order, buf_num,
        static_cast<int64_t>(buf.chunks_committed_out_of_order()));
    storage->SetIndexedStats(stats::traced_buf_write_wrap_count, buf_num,
                             static_cast<int64_t>(buf.write_wrap_count()));
    storage->SetIndexedStats(stats::traced_buf_patches_succeeded, buf_num,
                             static_cast<int64_t>(buf.patches_succeeded()));
    storage->SetIndexedStats(stats::traced_buf_patches_failed, buf_num,
                             static_cast<int64_t>(buf.patches_failed()));
    storage->SetIndexedStats(stats::traced_buf_readaheads_succeeded, buf_num,
                             static_cast<int64_t>(buf.readaheads_succeeded()));
    storage->SetIndexedStats(stats::traced_buf_readaheads_failed, buf_num,
                             static_cast<int64_t>(buf.readaheads_failed()));
    storage->SetIndexedStats(
        stats::traced_buf_trace_writer_packet_loss, buf_num,
        static_cast<int64_t>(buf.trace_writer_packet_loss()));
  }
}

void ProtoTraceParser::ParseProfilePacket(
    int64_t ts,
    PacketSequenceStateGeneration* sequence_state,
    uint32_t seq_id,
    ConstBytes blob) {
  protos::pbzero::ProfilePacket::Decoder packet(blob.data, blob.size);
  context_->heap_profile_tracker->SetProfilePacketIndex(seq_id, packet.index());

  for (auto it = packet.strings(); it; ++it) {
    protos::pbzero::InternedString::Decoder entry(*it);

    const char* str = reinterpret_cast<const char*>(entry.str().data);
    auto str_view = base::StringView(str, entry.str().size);
    sequence_state->state()->sequence_stack_profile_tracker().AddString(
        entry.iid(), str_view);
  }

  for (auto it = packet.mappings(); it; ++it) {
    protos::pbzero::Mapping::Decoder entry(*it);
    SequenceStackProfileTracker::SourceMapping src_mapping =
        ProfilePacketUtils::MakeSourceMapping(entry);
    sequence_state->state()->sequence_stack_profile_tracker().AddMapping(
        entry.iid(), src_mapping);
  }

  for (auto it = packet.frames(); it; ++it) {
    protos::pbzero::Frame::Decoder entry(*it);
    SequenceStackProfileTracker::SourceFrame src_frame =
        ProfilePacketUtils::MakeSourceFrame(entry);
    sequence_state->state()->sequence_stack_profile_tracker().AddFrame(
        entry.iid(), src_frame);
  }

  for (auto it = packet.callstacks(); it; ++it) {
    protos::pbzero::Callstack::Decoder entry(*it);
    SequenceStackProfileTracker::SourceCallstack src_callstack =
        ProfilePacketUtils::MakeSourceCallstack(entry);
    sequence_state->state()->sequence_stack_profile_tracker().AddCallstack(
        entry.iid(), src_callstack);
  }

  for (auto it = packet.process_dumps(); it; ++it) {
    protos::pbzero::ProfilePacket::ProcessHeapSamples::Decoder entry(*it);

    auto maybe_timestamp = context_->clock_tracker->ToTraceTime(
        protos::pbzero::BUILTIN_CLOCK_MONOTONIC_COARSE,
        static_cast<int64_t>(entry.timestamp()));

    // ToTraceTime() increments the clock_sync_failure error stat in this case.
    if (!maybe_timestamp)
      continue;

    int64_t timestamp = *maybe_timestamp;

    int pid = static_cast<int>(entry.pid());
    context_->storage->SetIndexedStats(stats::heapprofd_last_profile_timestamp,
                                       pid, ts);

    if (entry.disconnected())
      context_->storage->IncrementIndexedStats(
          stats::heapprofd_client_disconnected, pid);
    if (entry.buffer_corrupted())
      context_->storage->IncrementIndexedStats(
          stats::heapprofd_buffer_corrupted, pid);
    if (entry.buffer_overran() ||
        entry.client_error() ==
            protos::pbzero::ProfilePacket::ProcessHeapSamples::
                CLIENT_ERROR_HIT_TIMEOUT) {
      context_->storage->IncrementIndexedStats(stats::heapprofd_buffer_overran,
                                               pid);
    }
    if (entry.client_error()) {
      context_->storage->SetIndexedStats(stats::heapprofd_client_error, pid,
                                         entry.client_error());
    }
    if (entry.rejected_concurrent())
      context_->storage->IncrementIndexedStats(
          stats::heapprofd_rejected_concurrent, pid);
    if (entry.hit_guardrail())
      context_->storage->IncrementIndexedStats(stats::heapprofd_hit_guardrail,
                                               pid);
    if (entry.orig_sampling_interval_bytes()) {
      context_->storage->SetIndexedStats(
          stats::heapprofd_sampling_interval_adjusted, pid,
          static_cast<int64_t>(entry.sampling_interval_bytes()) -
              static_cast<int64_t>(entry.orig_sampling_interval_bytes()));
    }

    protos::pbzero::ProfilePacket::ProcessStats::Decoder stats(entry.stats());
    context_->storage->IncrementIndexedStats(
        stats::heapprofd_unwind_time_us, static_cast<int>(entry.pid()),
        static_cast<int64_t>(stats.total_unwinding_time_us()));
    context_->storage->IncrementIndexedStats(
        stats::heapprofd_unwind_samples, static_cast<int>(entry.pid()),
        static_cast<int64_t>(stats.heap_samples()));
    context_->storage->IncrementIndexedStats(
        stats::heapprofd_client_spinlock_blocked, static_cast<int>(entry.pid()),
        static_cast<int64_t>(stats.client_spinlock_blocked_us()));

    // orig_sampling_interval_bytes was introduced slightly after a bug with
    // self_max_count was fixed in the producer. We use this as a proxy
    // whether or not we are getting this data from a fixed producer or not.
    bool trustworthy_max_count = entry.orig_sampling_interval_bytes() > 0;

    for (auto sample_it = entry.samples(); sample_it; ++sample_it) {
      protos::pbzero::ProfilePacket::HeapSample::Decoder sample(*sample_it);

      HeapProfileTracker::SourceAllocation src_allocation;
      src_allocation.pid = entry.pid();
      if (entry.heap_name().size != 0) {
        src_allocation.heap_name =
            context_->storage->InternString(entry.heap_name());
      } else {
        src_allocation.heap_name = context_->storage->InternString("malloc");
      }
      src_allocation.timestamp = timestamp;
      src_allocation.callstack_id = sample.callstack_id();
      if (sample.has_self_max()) {
        src_allocation.self_allocated = sample.self_max();
        if (trustworthy_max_count)
          src_allocation.alloc_count = sample.self_max_count();
      } else {
        src_allocation.self_allocated = sample.self_allocated();
        src_allocation.self_freed = sample.self_freed();
        src_allocation.alloc_count = sample.alloc_count();
        src_allocation.free_count = sample.free_count();
      }

      context_->heap_profile_tracker->StoreAllocation(seq_id, src_allocation);
    }
  }
  if (!packet.continued()) {
    PERFETTO_CHECK(sequence_state);
    ProfilePacketInternLookup intern_lookup(sequence_state);
    context_->heap_profile_tracker->FinalizeProfile(
        seq_id, &sequence_state->state()->sequence_stack_profile_tracker(),
        &intern_lookup);
  }
}

void ProtoTraceParser::ParseDeobfuscationMapping(int64_t,
                                                 PacketSequenceStateGeneration*,
                                                 uint32_t /* seq_id */,
                                                 ConstBytes blob) {
  protos::pbzero::DeobfuscationMapping::Decoder deobfuscation_mapping(
      blob.data, blob.size);
  if (deobfuscation_mapping.package_name().size == 0)
    return;

  auto opt_package_name_id = context_->storage->string_pool().GetId(
      deobfuscation_mapping.package_name());
  auto opt_memfd_id = context_->storage->string_pool().GetId("memfd");
  if (!opt_package_name_id && !opt_memfd_id)
    return;

  for (auto class_it = deobfuscation_mapping.obfuscated_classes(); class_it;
       ++class_it) {
    protos::pbzero::ObfuscatedClass::Decoder cls(*class_it);
    for (auto member_it = cls.obfuscated_methods(); member_it; ++member_it) {
      protos::pbzero::ObfuscatedMember::Decoder member(*member_it);
      std::string merged_obfuscated = cls.obfuscated_name().ToStdString() +
                                      "." +
                                      member.obfuscated_name().ToStdString();
      auto merged_obfuscated_id = context_->storage->string_pool().GetId(
          base::StringView(merged_obfuscated));
      if (!merged_obfuscated_id)
        continue;
      std::string merged_deobfuscated =
          FullyQualifiedDeobfuscatedName(cls, member);

      std::vector<tables::StackProfileFrameTable::Id> frames;
      if (opt_package_name_id) {
        const std::vector<tables::StackProfileFrameTable::Id>* pkg_frames =
            context_->global_stack_profile_tracker->JavaFramesForName(
                {*merged_obfuscated_id, *opt_package_name_id});
        if (pkg_frames) {
          frames.insert(frames.end(), pkg_frames->begin(), pkg_frames->end());
        }
      }
      if (opt_memfd_id) {
        const std::vector<tables::StackProfileFrameTable::Id>* memfd_frames =
            context_->global_stack_profile_tracker->JavaFramesForName(
                {*merged_obfuscated_id, *opt_memfd_id});
        if (memfd_frames) {
          frames.insert(frames.end(), memfd_frames->begin(),
                        memfd_frames->end());
        }
      }

      for (tables::StackProfileFrameTable::Id frame_id : frames) {
        auto* frames_tbl =
            context_->storage->mutable_stack_profile_frame_table();
        frames_tbl->mutable_deobfuscated_name()->Set(
            *frames_tbl->id().IndexOf(frame_id),
            context_->storage->InternString(
                base::StringView(merged_deobfuscated)));
      }
    }
  }
}

void ProtoTraceParser::ParseChromeEvents(int64_t ts, ConstBytes blob) {
  TraceStorage* storage = context_->storage.get();
  protos::pbzero::ChromeEventBundle::Decoder bundle(blob.data, blob.size);
  ArgsTracker args(context_);
  if (bundle.has_metadata()) {
    RawId id = storage->mutable_raw_table()
                   ->Insert({ts, raw_chrome_metadata_event_id_, 0, 0})
                   .id;
    auto inserter = args.AddArgsTo(id);

    uint32_t bundle_index =
        context_->metadata_tracker->IncrementChromeMetadataBundleCount();

    // The legacy untyped metadata is proxied via a special event in the raw
    // table to JSON export.
    for (auto it = bundle.metadata(); it; ++it) {
      protos::pbzero::ChromeMetadata::Decoder metadata(*it);
      Variadic value;
      if (metadata.has_string_value()) {
        value =
            Variadic::String(storage->InternString(metadata.string_value()));
      } else if (metadata.has_int_value()) {
        value = Variadic::Integer(metadata.int_value());
      } else if (metadata.has_bool_value()) {
        value = Variadic::Integer(metadata.bool_value());
      } else if (metadata.has_json_value()) {
        value = Variadic::Json(storage->InternString(metadata.json_value()));
      } else {
        context_->storage->IncrementStats(stats::empty_chrome_metadata);
        continue;
      }

      StringId name_id = storage->InternString(metadata.name());
      args.AddArgsTo(id).AddArg(name_id, value);

      char buffer[2048];
      base::StringWriter writer(buffer, sizeof(buffer));
      writer.AppendString("cr-");
      // If we have data from multiple Chrome instances, append a suffix
      // to differentiate them.
      if (bundle_index > 1) {
        writer.AppendUnsignedInt(bundle_index);
        writer.AppendChar('-');
      }
      writer.AppendString(metadata.name());

      auto metadata_id = storage->InternString(writer.GetStringView());
      context_->metadata_tracker->SetDynamicMetadata(metadata_id, value);
    }
  }

  if (bundle.has_legacy_ftrace_output()) {
    RawId id =
        storage->mutable_raw_table()
            ->Insert({ts, raw_chrome_legacy_system_trace_event_id_, 0, 0})
            .id;

    std::string data;
    for (auto it = bundle.legacy_ftrace_output(); it; ++it) {
      data += (*it).ToStdString();
    }
    Variadic value =
        Variadic::String(storage->InternString(base::StringView(data)));
    args.AddArgsTo(id).AddArg(data_name_id_, value);
  }

  if (bundle.has_legacy_json_trace()) {
    for (auto it = bundle.legacy_json_trace(); it; ++it) {
      protos::pbzero::ChromeLegacyJsonTrace::Decoder legacy_trace(*it);
      if (legacy_trace.type() !=
          protos::pbzero::ChromeLegacyJsonTrace::USER_TRACE) {
        continue;
      }
      RawId id =
          storage->mutable_raw_table()
              ->Insert({ts, raw_chrome_legacy_user_trace_event_id_, 0, 0})
              .id;
      Variadic value =
          Variadic::String(storage->InternString(legacy_trace.data()));
      args.AddArgsTo(id).AddArg(data_name_id_, value);
    }
  }
}

void ProtoTraceParser::ParseMetatraceEvent(int64_t ts, ConstBytes blob) {
  protos::pbzero::PerfettoMetatrace::Decoder event(blob.data, blob.size);
  auto utid = context_->process_tracker->GetOrCreateThread(event.thread_id());

  StringId cat_id = metatrace_id_;
  StringId name_id = kNullStringId;
  char fallback[64];

  // This function inserts the args from the proto into the args table.
  // Args inserted with the same key multiple times are treated as an array:
  // this function correctly creates the key and flat key for each arg array.
  auto args_fn = [this, &event](ArgsTracker::BoundInserter* inserter) {
    using Arg = std::pair<StringId, StringId>;

    // First, get a list of all the args so we can group them by key.
    std::vector<Arg> interned;
    for (auto it = event.args(); it; ++it) {
      protos::pbzero::PerfettoMetatrace::Arg::Decoder arg_proto(*it);
      StringId key = context_->storage->InternString(arg_proto.key());
      StringId value = context_->storage->InternString(arg_proto.value());
      interned.emplace_back(key, value);
    }

    // We stable sort insted of sorting here to avoid changing the order of the
    // args in arrays.
    std::stable_sort(interned.begin(), interned.end(),
                     [](const Arg& a, const Arg& b) {
                       return a.first.raw_id() < b.second.raw_id();
                     });

    // Compute the correct key for each arg, possibly adding an index to
    // the end of the key if needed.
    char buffer[2048];
    uint32_t current_idx = 0;
    for (auto it = interned.begin(); it != interned.end(); ++it) {
      auto next = it + 1;
      StringId key = it->first;
      StringId next_key = next == interned.end() ? kNullStringId : next->first;

      if (key != next_key && current_idx == 0) {
        inserter->AddArg(key, Variadic::String(it->second));
      } else {
        constexpr size_t kMaxIndexSize = 20;
        base::StringView key_str = context_->storage->GetString(key);
        if (key_str.size() >= sizeof(buffer) - kMaxIndexSize) {
          PERFETTO_DLOG("Ignoring arg with unreasonbly large size");
          continue;
        }

        base::StringWriter writer(buffer, sizeof(buffer));
        writer.AppendString(key_str);
        writer.AppendChar('[');
        writer.AppendUnsignedInt(current_idx);
        writer.AppendChar(']');

        StringId new_key =
            context_->storage->InternString(writer.GetStringView());
        inserter->AddArg(key, new_key, Variadic::String(it->second));

        current_idx = key == next_key ? current_idx + 1 : 0;
      }
    }
  };

  if (event.has_event_id() || event.has_event_name()) {
    if (event.has_event_id()) {
      auto eid = event.event_id();
      if (eid < metatrace::EVENTS_MAX) {
        name_id = context_->storage->InternString(metatrace::kEventNames[eid]);
      } else {
        sprintf(fallback, "Event %d", eid);
        name_id = context_->storage->InternString(fallback);
      }
    } else {
      name_id = context_->storage->InternString(event.event_name());
    }
    TrackId track_id = context_->track_tracker->InternThreadTrack(utid);
    context_->slice_tracker->Scoped(ts, track_id, cat_id, name_id,
                                    event.event_duration_ns(), args_fn);
  } else if (event.has_counter_id() || event.has_counter_name()) {
    if (event.has_counter_id()) {
      auto cid = event.counter_id();
      if (cid < metatrace::COUNTERS_MAX) {
        name_id =
            context_->storage->InternString(metatrace::kCounterNames[cid]);
      } else {
        sprintf(fallback, "Counter %d", cid);
        name_id = context_->storage->InternString(fallback);
      }
    } else {
      name_id = context_->storage->InternString(event.counter_name());
    }
    TrackId track =
        context_->track_tracker->InternThreadCounterTrack(name_id, utid);
    auto opt_id =
        context_->event_tracker->PushCounter(ts, event.counter_value(), track);
    if (opt_id) {
      auto inserter = context_->args_tracker->AddArgsTo(*opt_id);
      args_fn(&inserter);
    }
  }

  if (event.has_overruns())
    context_->storage->IncrementStats(stats::metatrace_overruns);
}

void ProtoTraceParser::ParseTraceConfig(ConstBytes blob) {
  protos::pbzero::TraceConfig::Decoder trace_config(blob.data, blob.size);

  // TODO(eseckler): Propagate statuses from modules.
  for (auto& module : context_->modules) {
    module->ParseTraceConfig(trace_config);
  }

  int64_t uuid_msb = trace_config.trace_uuid_msb();
  int64_t uuid_lsb = trace_config.trace_uuid_lsb();
  if (uuid_msb != 0 || uuid_lsb != 0) {
    base::Uuid uuid(uuid_lsb, uuid_msb);
    std::string str = uuid.ToPrettyString();
    StringId id = context_->storage->InternString(base::StringView(str));
    context_->metadata_tracker->SetMetadata(metadata::trace_uuid,
                                            Variadic::String(id));
  }

  if (trace_config.has_unique_session_name()) {
    StringId id = context_->storage->InternString(
        base::StringView(trace_config.unique_session_name()));
    context_->metadata_tracker->SetMetadata(metadata::unique_session_name,
                                            Variadic::String(id));
  }

  DescriptorPool pool;
  pool.AddFromFileDescriptorSet(kConfigDescriptor.data(),
                                kConfigDescriptor.size());

  std::string text = protozero_to_text::ProtozeroToText(
      pool, ".perfetto.protos.TraceConfig", blob,
      protozero_to_text::kIncludeNewLines);
  StringId id = context_->storage->InternString(base::StringView(text));
  context_->metadata_tracker->SetMetadata(metadata::trace_config_pbtxt,
                                          Variadic::String(id));
}

void ProtoTraceParser::ParseModuleSymbols(ConstBytes blob) {
  protos::pbzero::ModuleSymbols::Decoder module_symbols(blob.data, blob.size);
  StringId build_id;
  // TODO(b/148109467): Remove workaround once all active Chrome versions
  // write raw bytes instead of a string as build_id.
  if (module_symbols.build_id().size == 33) {
    build_id = context_->storage->InternString(module_symbols.build_id());
  } else {
    build_id = context_->storage->InternString(base::StringView(base::ToHex(
        module_symbols.build_id().data, module_symbols.build_id().size)));
  }

  auto mapping_ids = context_->global_stack_profile_tracker->FindMappingRow(
      context_->storage->InternString(module_symbols.path()), build_id);
  if (mapping_ids.empty()) {
    context_->storage->IncrementStats(stats::stackprofile_invalid_mapping_id);
    return;
  }
  for (auto addr_it = module_symbols.address_symbols(); addr_it; ++addr_it) {
    protos::pbzero::AddressSymbols::Decoder address_symbols(*addr_it);

    uint32_t symbol_set_id = context_->storage->symbol_table().row_count();

    bool has_lines = false;
    for (auto line_it = address_symbols.lines(); line_it; ++line_it) {
      protos::pbzero::Line::Decoder line(*line_it);
      context_->storage->mutable_symbol_table()->Insert(
          {symbol_set_id, context_->storage->InternString(line.function_name()),
           context_->storage->InternString(line.source_file_name()),
           line.line_number()});
      has_lines = true;
    }
    if (!has_lines) {
      continue;
    }
    bool frame_found = false;
    for (MappingId mapping_id : mapping_ids) {
      std::vector<FrameId> frame_ids =
          context_->global_stack_profile_tracker->FindFrameIds(
              mapping_id, address_symbols.address());

      for (const FrameId frame_id : frame_ids) {
        auto* frames = context_->storage->mutable_stack_profile_frame_table();
        uint32_t frame_row = *frames->id().IndexOf(frame_id);
        frames->mutable_symbol_set_id()->Set(frame_row, symbol_set_id);
        frame_found = true;
      }
    }

    if (!frame_found) {
      context_->storage->IncrementStats(stats::stackprofile_invalid_frame_id);
      continue;
    }
  }
}

void ProtoTraceParser::ParseSmapsPacket(int64_t ts, ConstBytes blob) {
  protos::pbzero::SmapsPacket::Decoder sp(blob.data, blob.size);
  auto upid = context_->process_tracker->GetOrCreateProcess(sp.pid());

  for (auto it = sp.entries(); it; ++it) {
    protos::pbzero::SmapsEntry::Decoder e(*it);
    context_->storage->mutable_profiler_smaps_table()->Insert(
        {upid, ts, context_->storage->InternString(e.path()),
         static_cast<int64_t>(e.size_kb()),
         static_cast<int64_t>(e.private_dirty_kb()),
         static_cast<int64_t>(e.swap_kb()),
         context_->storage->InternString(e.file_name()),
         static_cast<int64_t>(e.start_address()),
         static_cast<int64_t>(e.module_timestamp()),
         context_->storage->InternString(e.module_debugid()),
         context_->storage->InternString(e.module_debug_path()),
         static_cast<int32_t>(e.protection_flags()),
         static_cast<int64_t>(e.private_clean_resident_kb()),
         static_cast<int64_t>(e.shared_dirty_resident_kb()),
         static_cast<int64_t>(e.shared_clean_resident_kb()),
         static_cast<int64_t>(e.locked_kb()),
         static_cast<int64_t>(e.proportional_resident_kb())});
  }
}

}  // namespace trace_processor
}  // namespace perfetto

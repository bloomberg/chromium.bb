/*
 * Copyright (C) 2019 The Android Open Source Project
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

#ifndef INCLUDE_PERFETTO_TRACING_INTERNAL_TRACK_EVENT_INTERNAL_H_
#define INCLUDE_PERFETTO_TRACING_INTERNAL_TRACK_EVENT_INTERNAL_H_

#include "perfetto/base/flat_set.h"
#include "perfetto/protozero/scattered_heap_buffer.h"
#include "perfetto/tracing/core/forward_decls.h"
#include "perfetto/tracing/data_source.h"
#include "perfetto/tracing/debug_annotation.h"
#include "perfetto/tracing/trace_writer_base.h"
#include "perfetto/tracing/traced_value.h"
#include "perfetto/tracing/track.h"
#include "protos/perfetto/common/builtin_clock.pbzero.h"
#include "protos/perfetto/trace/interned_data/interned_data.pbzero.h"
#include "protos/perfetto/trace/track_event/track_event.pbzero.h"

#include <unordered_map>

namespace perfetto {

// Represents a point in time for the clock specified by |clock_id|.
struct TraceTimestamp {
  // Clock IDs have the following semantic:
  // [1, 63]:    Builtin types, see BuiltinClock from
  //             ../common/builtin_clock.proto.
  // [64, 127]:  User-defined clocks. These clocks are sequence-scoped. They
  //             are only valid within the same |trusted_packet_sequence_id|
  //             (i.e. only for TracePacket(s) emitted by the same TraceWriter
  //             that emitted the clock snapshot).
  // [128, MAX]: Reserved for future use. The idea is to allow global clock
  //             IDs and setting this ID to hash(full_clock_name) & ~127.
  // Learn more: `clock_snapshot.proto`
  uint32_t clock_id;
  uint64_t value;
};

class EventContext;
class TrackEventSessionObserver;
struct Category;
struct TraceTimestamp;
namespace protos {
namespace gen {
class TrackEventConfig;
}  // namespace gen
namespace pbzero {
class DebugAnnotation;
}  // namespace pbzero
}  // namespace protos

// A callback interface for observing track event tracing sessions starting and
// stopping. See TrackEvent::{Add,Remove}SessionObserver. Note that all methods
// will be called on an internal Perfetto thread.
class PERFETTO_EXPORT TrackEventSessionObserver {
 public:
  virtual ~TrackEventSessionObserver();
  // Called when a track event tracing session is configured. Note tracing isn't
  // active yet, so track events emitted here won't be recorded. See
  // DataSourceBase::OnSetup.
  virtual void OnSetup(const DataSourceBase::SetupArgs&);
  // Called when a track event tracing session is started. It is possible to
  // emit track events from this callback.
  virtual void OnStart(const DataSourceBase::StartArgs&);
  // Called when a track event tracing session is stopped. It is still possible
  // to emit track events from this callback.
  virtual void OnStop(const DataSourceBase::StopArgs&);
};

namespace internal {
class TrackEventCategoryRegistry;

class PERFETTO_EXPORT BaseTrackEventInternedDataIndex {
 public:
  virtual ~BaseTrackEventInternedDataIndex();

#if PERFETTO_DCHECK_IS_ON()
  const char* type_id_ = nullptr;
  const void* add_function_ptr_ = nullptr;
#endif  // PERFETTO_DCHECK_IS_ON()
};

struct TrackEventTlsState {
  template <typename TraceContext>
  explicit TrackEventTlsState(const TraceContext& trace_context) {
    auto locked_ds = trace_context.GetDataSourceLocked();
    if (locked_ds.valid()) {
      const auto& config = locked_ds->GetConfig();
      disable_incremental_timestamps = config.disable_incremental_timestamps();
      filter_debug_annotations = config.filter_debug_annotations();
    }
  }
  bool disable_incremental_timestamps = false;
  bool filter_debug_annotations = false;
};

struct TrackEventIncrementalState {
  static constexpr size_t kMaxInternedDataFields = 32;

  // Packet-sequence-scoped clock that encodes microsecond timestamps in the
  // domain of the clock returned by GetClockId() as delta values - see
  // Clock::is_incremental in perfetto/trace/clock_snapshot.proto.
  // Default unit: nanoseconds.
  static constexpr uint32_t kClockIdIncremental = 64;

  bool was_cleared = true;

  // A heap-allocated message for storing newly seen interned data while we are
  // in the middle of writing a track event. When a track event wants to write
  // new interned data into the trace, it is first serialized into this message
  // and then flushed to the real trace in EventContext when the packet ends.
  // The message is cached here as a part of incremental state so that we can
  // reuse the underlying buffer allocation for subsequently written interned
  // data.
  protozero::HeapBuffered<protos::pbzero::InternedData>
      serialized_interned_data;

  // In-memory indices for looking up interned data ids.
  // For each intern-able field (up to a max of 32) we keep a dictionary of
  // field-value -> interning-key. Depending on the type we either keep the full
  // value or a hash of it (See track_event_interned_data_index.h)
  using InternedDataIndex =
      std::pair</* interned_data.proto field number */ size_t,
                std::unique_ptr<BaseTrackEventInternedDataIndex>>;
  std::array<InternedDataIndex, kMaxInternedDataFields> interned_data_indices =
      {};

  // Track uuids for which we have written descriptors into the trace. If a
  // trace event uses a track which is not in this set, we'll write out a
  // descriptor for it.
  base::FlatSet<uint64_t> seen_tracks;

  // Dynamically registered category names that have been encountered during
  // this tracing session. The value in the map indicates whether the category
  // is enabled or disabled.
  std::unordered_map<std::string, bool> dynamic_categories;

  // The latest reference timestamp that was used in a TracePacket or in a
  // ClockSnapshot. The increment between this timestamp and the current trace
  // time (GetTimeNs) is a value in kClockIdIncremental's domain.
  uint64_t last_timestamp_ns = 0;

  // The latest known counter values that was used in a TracePacket for each
  // counter track. The key (uint64_t) is the uuid of counter track.
  // The value is used for delta encoding of counter values.
  std::unordered_map<uint64_t, int64_t> last_counter_value_per_track;
};

// The backend portion of the track event trace point implemention. Outlined to
// a separate .cc file so it can be shared by different track event category
// namespaces.
class PERFETTO_EXPORT TrackEventInternal {
 public:
  static bool Initialize(
      const TrackEventCategoryRegistry&,
      bool (*register_data_source)(const DataSourceDescriptor&));

  static bool AddSessionObserver(TrackEventSessionObserver*);
  static void RemoveSessionObserver(TrackEventSessionObserver*);

  static void EnableTracing(const TrackEventCategoryRegistry& registry,
                            const protos::gen::TrackEventConfig& config,
                            const DataSourceBase::SetupArgs&);
  static void OnStart(const DataSourceBase::StartArgs&);
  static void DisableTracing(const TrackEventCategoryRegistry& registry,
                             const DataSourceBase::StopArgs&);
  static bool IsCategoryEnabled(const TrackEventCategoryRegistry& registry,
                                const protos::gen::TrackEventConfig& config,
                                const Category& category);

  static perfetto::EventContext WriteEvent(
      TraceWriterBase*,
      TrackEventIncrementalState*,
      const TrackEventTlsState& tls_state,
      const Category* category,
      const char* name,
      perfetto::protos::pbzero::TrackEvent::Type,
      const TraceTimestamp& timestamp);

  static void ResetIncrementalStateIfRequired(
      TraceWriterBase* trace_writer,
      TrackEventIncrementalState* incr_state,
      const TrackEventTlsState& tls_state,
      const TraceTimestamp& timestamp) {
    if (incr_state->was_cleared) {
      incr_state->was_cleared = false;
      ResetIncrementalState(trace_writer, incr_state, tls_state, timestamp);
    }
  }

  // TODO(altimin): Remove this method once Chrome uses
  // EventContext::AddDebugAnnotation directly.
  template <typename T>
  static void AddDebugAnnotation(perfetto::EventContext* event_ctx,
                                 const char* name,
                                 T&& value) {
    auto annotation = AddDebugAnnotation(event_ctx, name);
    WriteIntoTracedValue(
        internal::CreateTracedValueFromProto(annotation, event_ctx),
        std::forward<T>(value));
  }

  // If the given track hasn't been seen by the trace writer yet, write a
  // descriptor for it into the trace. Doesn't take a lock unless the track
  // descriptor is new.
  template <typename TrackType>
  static void WriteTrackDescriptorIfNeeded(
      const TrackType& track,
      TraceWriterBase* trace_writer,
      TrackEventIncrementalState* incr_state,
      const TrackEventTlsState& tls_state,
      const TraceTimestamp& timestamp) {
    auto it_and_inserted = incr_state->seen_tracks.insert(track.uuid);
    if (PERFETTO_LIKELY(!it_and_inserted.second))
      return;
    WriteTrackDescriptor(track, trace_writer, incr_state, tls_state, timestamp);
  }

  // Unconditionally write a track descriptor into the trace.
  template <typename TrackType>
  static void WriteTrackDescriptor(const TrackType& track,
                                   TraceWriterBase* trace_writer,
                                   TrackEventIncrementalState* incr_state,
                                   const TrackEventTlsState& tls_state,
                                   const TraceTimestamp& timestamp) {
    ResetIncrementalStateIfRequired(trace_writer, incr_state, tls_state,
                                    timestamp);
    TrackRegistry::Get()->SerializeTrack(
        track, NewTracePacket(trace_writer, incr_state, tls_state, timestamp));
  }

  // Get the current time in nanoseconds in the trace clock timebase.
  static uint64_t GetTimeNs();

  static TraceTimestamp GetTraceTime();

  // Get the clock used by GetTimeNs().
  static constexpr protos::pbzero::BuiltinClock GetClockId() {
#if !PERFETTO_BUILDFLAG(PERFETTO_OS_APPLE) && \
    !PERFETTO_BUILDFLAG(PERFETTO_OS_WIN)
    return protos::pbzero::BUILTIN_CLOCK_BOOTTIME;
#else
    return protos::pbzero::BUILTIN_CLOCK_MONOTONIC;
#endif
  }

  static int GetSessionCount();

  // Represents the default track for the calling thread.
  static const Track kDefaultTrack;

 private:
  static void ResetIncrementalState(TraceWriterBase* trace_writer,
                                    TrackEventIncrementalState* incr_state,
                                    const TrackEventTlsState& tls_state,
                                    const TraceTimestamp& timestamp);

  static protozero::MessageHandle<protos::pbzero::TracePacket> NewTracePacket(
      TraceWriterBase*,
      TrackEventIncrementalState*,
      const TrackEventTlsState& tls_state,
      TraceTimestamp,
      uint32_t seq_flags =
          protos::pbzero::TracePacket::SEQ_NEEDS_INCREMENTAL_STATE);
  static protos::pbzero::DebugAnnotation* AddDebugAnnotation(
      perfetto::EventContext*,
      const char* name);

  static std::atomic<int> session_count_;
};

}  // namespace internal
}  // namespace perfetto

#endif  // INCLUDE_PERFETTO_TRACING_INTERNAL_TRACK_EVENT_INTERNAL_H_

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

#include "perfetto/tracing/internal/track_event_internal.h"

#include "perfetto/base/proc_utils.h"
#include "perfetto/base/thread_utils.h"
#include "perfetto/base/time.h"
#include "perfetto/tracing/core/data_source_config.h"
#include "perfetto/tracing/internal/track_event_interned_fields.h"
#include "perfetto/tracing/track_event.h"
#include "perfetto/tracing/track_event_category_registry.h"
#include "perfetto/tracing/track_event_interned_data_index.h"
#include "protos/perfetto/common/data_source_descriptor.gen.h"
#include "protos/perfetto/common/track_event_descriptor.pbzero.h"
#include "protos/perfetto/trace/clock_snapshot.pbzero.h"
#include "protos/perfetto/trace/interned_data/interned_data.pbzero.h"
#include "protos/perfetto/trace/trace_packet_defaults.pbzero.h"
#include "protos/perfetto/trace/track_event/debug_annotation.pbzero.h"
#include "protos/perfetto/trace/track_event/track_descriptor.pbzero.h"

using perfetto::protos::pbzero::ClockSnapshot;

namespace perfetto {

TrackEventSessionObserver::~TrackEventSessionObserver() = default;
void TrackEventSessionObserver::OnSetup(const DataSourceBase::SetupArgs&) {}
void TrackEventSessionObserver::OnStart(const DataSourceBase::StartArgs&) {}
void TrackEventSessionObserver::OnStop(const DataSourceBase::StopArgs&) {}

namespace internal {

BaseTrackEventInternedDataIndex::~BaseTrackEventInternedDataIndex() = default;

namespace {

std::atomic<perfetto::base::PlatformThreadId> g_main_thread;
static constexpr const char kLegacySlowPrefix[] = "disabled-by-default-";
static constexpr const char kSlowTag[] = "slow";
static constexpr const char kDebugTag[] = "debug";
// Allows to specify a custom unit different than the default (ns) for
// the incremental clock.
// A multiplier of 1000 means that a timestamp = 3 should be
// interpreted as 3000 ns = 3 us.
// TODO(mohitms): Move it to TrackEventConfig.
constexpr uint64_t kIncrementalTimestampUnitMultiplier = 1;
static_assert(kIncrementalTimestampUnitMultiplier >= 1, "");

constexpr auto kClockIdIncremental =
    TrackEventIncrementalState::kClockIdIncremental;

void ForEachObserver(
    std::function<bool(TrackEventSessionObserver*&)> callback) {
  // Session observers, shared by all track event data source instances.
  static constexpr int kMaxObservers = 8;
  static std::recursive_mutex* mutex = new std::recursive_mutex{};  // Leaked.
  static std::array<TrackEventSessionObserver*, kMaxObservers> observers{};
  std::unique_lock<std::recursive_mutex> lock(*mutex);
  for (auto& o : observers) {
    if (!callback(o))
      break;
  }
}

enum class MatchType { kExact, kPattern };

bool NameMatchesPattern(const std::string& pattern,
                        const std::string& name,
                        MatchType match_type) {
  // To avoid pulling in all of std::regex, for now we only support a single "*"
  // wildcard at the end of the pattern.
  size_t i = pattern.find('*');
  if (i != std::string::npos) {
    PERFETTO_DCHECK(i == pattern.size() - 1);
    if (match_type != MatchType::kPattern)
      return false;
    return name.substr(0, i) == pattern.substr(0, i);
  }
  return name == pattern;
}

bool NameMatchesPatternList(const std::vector<std::string>& patterns,
                            const std::string& name,
                            MatchType match_type) {
  for (const auto& pattern : patterns) {
    if (NameMatchesPattern(pattern, name, match_type))
      return true;
  }
  return false;
}

}  // namespace

// static
const Track TrackEventInternal::kDefaultTrack{};

// static
std::atomic<int> TrackEventInternal::session_count_{};

// static
bool TrackEventInternal::Initialize(
    const TrackEventCategoryRegistry& registry,
    bool (*register_data_source)(const DataSourceDescriptor&)) {
  if (!g_main_thread)
    g_main_thread = perfetto::base::GetThreadId();

  DataSourceDescriptor dsd;
  dsd.set_name("track_event");

  protozero::HeapBuffered<protos::pbzero::TrackEventDescriptor> ted;
  for (size_t i = 0; i < registry.category_count(); i++) {
    auto category = registry.GetCategory(i);
    // Don't register group categories.
    if (category->IsGroup())
      continue;
    auto cat = ted->add_available_categories();
    cat->set_name(category->name);
    if (category->description)
      cat->set_description(category->description);
    for (const auto& tag : category->tags) {
      if (tag)
        cat->add_tags(tag);
    }
    // Disabled-by-default categories get a "slow" tag.
    if (!strncmp(category->name, kLegacySlowPrefix, strlen(kLegacySlowPrefix)))
      cat->add_tags(kSlowTag);
  }
  dsd.set_track_event_descriptor_raw(ted.SerializeAsString());

  return register_data_source(dsd);
}

// static
bool TrackEventInternal::AddSessionObserver(
    TrackEventSessionObserver* observer) {
  bool result = false;
  ForEachObserver([&](TrackEventSessionObserver*& o) {
    if (!o) {
      o = observer;
      result = true;
      return false;
    }
    return true;
  });
  return result;
}

// static
void TrackEventInternal::RemoveSessionObserver(
    TrackEventSessionObserver* observer) {
  ForEachObserver([&](TrackEventSessionObserver*& o) {
    if (o == observer) {
      o = nullptr;
      return false;
    }
    return true;
  });
}

// static
void TrackEventInternal::EnableTracing(
    const TrackEventCategoryRegistry& registry,
    const protos::gen::TrackEventConfig& config,
    const DataSourceBase::SetupArgs& args) {
  for (size_t i = 0; i < registry.category_count(); i++) {
    if (IsCategoryEnabled(registry, config, *registry.GetCategory(i)))
      registry.EnableCategoryForInstance(i, args.internal_instance_index);
  }
  ForEachObserver([&](TrackEventSessionObserver*& o) {
    if (o)
      o->OnSetup(args);
    return true;
  });
}

// static
void TrackEventInternal::OnStart(const DataSourceBase::StartArgs& args) {
  session_count_.fetch_add(1);
  ForEachObserver([&](TrackEventSessionObserver*& o) {
    if (o)
      o->OnStart(args);
    return true;
  });
}

// static
void TrackEventInternal::DisableTracing(
    const TrackEventCategoryRegistry& registry,
    const DataSourceBase::StopArgs& args) {
  ForEachObserver([&](TrackEventSessionObserver*& o) {
    if (o)
      o->OnStop(args);
    return true;
  });
  for (size_t i = 0; i < registry.category_count(); i++)
    registry.DisableCategoryForInstance(i, args.internal_instance_index);
}

// static
bool TrackEventInternal::IsCategoryEnabled(
    const TrackEventCategoryRegistry& registry,
    const protos::gen::TrackEventConfig& config,
    const Category& category) {
  // If this is a group category, check if any of its constituent categories are
  // enabled. If so, then this one is enabled too.
  if (category.IsGroup()) {
    bool result = false;
    category.ForEachGroupMember([&](const char* member_name, size_t name_size) {
      for (size_t i = 0; i < registry.category_count(); i++) {
        const auto ref_category = registry.GetCategory(i);
        // Groups can't refer to other groups.
        if (ref_category->IsGroup())
          continue;
        // Require an exact match.
        if (ref_category->name_size() != name_size ||
            strncmp(ref_category->name, member_name, name_size)) {
          continue;
        }
        if (IsCategoryEnabled(registry, config, *ref_category)) {
          result = true;
          // Break ForEachGroupMember() loop.
          return false;
        }
        break;
      }
      // No match? Must be a dynamic category.
      DynamicCategory dyn_category(std::string(member_name, name_size));
      Category ref_category{Category::FromDynamicCategory(dyn_category)};
      if (IsCategoryEnabled(registry, config, ref_category)) {
        result = true;
        // Break ForEachGroupMember() loop.
        return false;
      }
      // No match found => keep iterating.
      return true;
    });
    return result;
  }

  auto has_matching_tag = [&](std::function<bool(const char*)> matcher) {
    for (const auto& tag : category.tags) {
      if (!tag)
        break;
      if (matcher(tag))
        return true;
    }
    // Legacy "disabled-by-default" categories automatically get the "slow" tag.
    if (!strncmp(category.name, kLegacySlowPrefix, strlen(kLegacySlowPrefix)) &&
        matcher(kSlowTag)) {
      return true;
    }
    return false;
  };

  // First try exact matches, then pattern matches.
  const std::array<MatchType, 2> match_types = {
      {MatchType::kExact, MatchType::kPattern}};
  for (auto match_type : match_types) {
    // 1. Enabled categories.
    if (NameMatchesPatternList(config.enabled_categories(), category.name,
                               match_type)) {
      return true;
    }

    // 2. Enabled tags.
    if (has_matching_tag([&](const char* tag) {
          return NameMatchesPatternList(config.enabled_tags(), tag, match_type);
        })) {
      return true;
    }

    // 3. Disabled categories.
    if (NameMatchesPatternList(config.disabled_categories(), category.name,
                               match_type)) {
      return false;
    }

    // 4. Disabled tags.
    if (has_matching_tag([&](const char* tag) {
          if (config.disabled_tags_size()) {
            return NameMatchesPatternList(config.disabled_tags(), tag,
                                          match_type);
          } else {
            // The "slow" and "debug" tags are disabled by default.
            return NameMatchesPattern(kSlowTag, tag, match_type) ||
                   NameMatchesPattern(kDebugTag, tag, match_type);
          }
        })) {
      return false;
    }
  }

  // If nothing matched, enable the category by default.
  return true;
}

// static
uint64_t TrackEventInternal::GetTimeNs() {
  if (GetClockId() == protos::pbzero::BUILTIN_CLOCK_BOOTTIME)
    return static_cast<uint64_t>(perfetto::base::GetBootTimeNs().count());
  PERFETTO_DCHECK(GetClockId() == protos::pbzero::BUILTIN_CLOCK_MONOTONIC);
  return static_cast<uint64_t>(perfetto::base::GetWallTimeNs().count());
}

// static
TraceTimestamp TrackEventInternal::GetTraceTime() {
  return {kClockIdIncremental, GetTimeNs()};
}

// static
int TrackEventInternal::GetSessionCount() {
  return session_count_.load();
}

// static
void TrackEventInternal::ResetIncrementalState(
    TraceWriterBase* trace_writer,
    TrackEventIncrementalState* incr_state,
    const TrackEventTlsState& tls_state,
    const TraceTimestamp& timestamp) {
  auto sequence_timestamp = timestamp;
  if (timestamp.clock_id != TrackEventInternal::GetClockId() &&
      timestamp.clock_id != kClockIdIncremental) {
    sequence_timestamp = TrackEventInternal::GetTraceTime();
  }

  incr_state->last_timestamp_ns = sequence_timestamp.value;
  auto default_track = ThreadTrack::Current();
  {
    // Mark any incremental state before this point invalid. Also set up
    // defaults so that we don't need to repeat constant data for each packet.
    auto packet = NewTracePacket(
        trace_writer, incr_state, tls_state, timestamp,
        protos::pbzero::TracePacket::SEQ_INCREMENTAL_STATE_CLEARED);
    auto defaults = packet->set_trace_packet_defaults();

    // Establish the default track for this event sequence.
    auto track_defaults = defaults->set_track_event_defaults();
    track_defaults->set_track_uuid(default_track.uuid);

    if (PERFETTO_LIKELY(!tls_state.disable_incremental_timestamps)) {
      defaults->set_timestamp_clock_id(kClockIdIncremental);
      ClockSnapshot* clocks = packet->set_clock_snapshot();
      // Trace clock.
      ClockSnapshot::Clock* trace_clock = clocks->add_clocks();
      trace_clock->set_clock_id(GetClockId());
      trace_clock->set_timestamp(sequence_timestamp.value);
      // Delta-encoded incremental clock in nano seconds.
      // TODO(b/168311581): Make the unit of this clock configurable to allow
      // trade-off between precision and encoded trace size.
      ClockSnapshot::Clock* clock_incremental = clocks->add_clocks();
      clock_incremental->set_clock_id(kClockIdIncremental);
      auto ts_unit_multiplier = kIncrementalTimestampUnitMultiplier;
      clock_incremental->set_timestamp(sequence_timestamp.value /
                                       ts_unit_multiplier);
      clock_incremental->set_is_incremental(true);
      clock_incremental->set_unit_multiplier_ns(ts_unit_multiplier);
    } else {
      defaults->set_timestamp_clock_id(GetClockId());
    }
  }

  // Every thread should write a descriptor for its default track, because most
  // trace points won't explicitly reference it. We also write the process
  // descriptor from every thread that writes trace events to ensure it gets
  // emitted at least once.
  WriteTrackDescriptor(default_track, trace_writer, incr_state, tls_state,
                       sequence_timestamp);

  WriteTrackDescriptor(ProcessTrack::Current(), trace_writer, incr_state,
                       tls_state, sequence_timestamp);
}

// static
protozero::MessageHandle<protos::pbzero::TracePacket>
TrackEventInternal::NewTracePacket(TraceWriterBase* trace_writer,
                                   TrackEventIncrementalState* incr_state,
                                   const TrackEventTlsState& tls_state,
                                   TraceTimestamp timestamp,
                                   uint32_t seq_flags) {
  if (PERFETTO_UNLIKELY(tls_state.disable_incremental_timestamps &&
                        timestamp.clock_id == kClockIdIncremental)) {
    timestamp.clock_id = GetClockId();
  }
  auto packet = trace_writer->NewTracePacket();
  // TODO(mohitms): Consider using kIncrementalTimestampUnitMultiplier.
  if (PERFETTO_LIKELY(timestamp.clock_id == kClockIdIncremental)) {
    if (PERFETTO_LIKELY(incr_state->last_timestamp_ns <= timestamp.value)) {
      // No need to set the clock id here, since kClockIdIncremental is the
      // clock id assumed by default.
      auto ts_unit_multiplier = kIncrementalTimestampUnitMultiplier;
      auto time_diff_ns = timestamp.value - incr_state->last_timestamp_ns;
      packet->set_timestamp(time_diff_ns / ts_unit_multiplier);
      incr_state->last_timestamp_ns = timestamp.value;
    } else {
      packet->set_timestamp(timestamp.value);
      packet->set_timestamp_clock_id(GetClockId());
    }
  } else {
    packet->set_timestamp(timestamp.value);
    auto default_clock = tls_state.disable_incremental_timestamps
                             ? static_cast<uint32_t>(GetClockId())
                             : kClockIdIncremental;
    if (PERFETTO_UNLIKELY(timestamp.clock_id != default_clock))
      packet->set_timestamp_clock_id(timestamp.clock_id);
  }
  packet->set_sequence_flags(seq_flags);
  return packet;
}

// static
EventContext TrackEventInternal::WriteEvent(
    TraceWriterBase* trace_writer,
    TrackEventIncrementalState* incr_state,
    const TrackEventTlsState& tls_state,
    const Category* category,
    const char* name,
    perfetto::protos::pbzero::TrackEvent::Type type,
    const TraceTimestamp& timestamp) {
  PERFETTO_DCHECK(g_main_thread);
  PERFETTO_DCHECK(!incr_state->was_cleared);
  auto packet = NewTracePacket(trace_writer, incr_state, tls_state, timestamp);
  EventContext ctx(std::move(packet), incr_state, &tls_state);

  auto track_event = ctx.event();
  if (type != protos::pbzero::TrackEvent::TYPE_UNSPECIFIED)
    track_event->set_type(type);

  // We assume that |category| and |name| point to strings with static lifetime.
  // This means we can use their addresses as interning keys.
  // TODO(skyostil): Intern categories at compile time.
  if (category && type != protos::pbzero::TrackEvent::TYPE_SLICE_END &&
      type != protos::pbzero::TrackEvent::TYPE_COUNTER) {
    category->ForEachGroupMember(
        [&](const char* member_name, size_t name_size) {
          size_t category_iid =
              InternedEventCategory::Get(&ctx, member_name, name_size);
          track_event->add_category_iids(category_iid);
          return true;
        });
  }
  if (name && type != protos::pbzero::TrackEvent::TYPE_SLICE_END) {
    size_t name_iid = InternedEventName::Get(&ctx, name);
    track_event->set_name_iid(name_iid);
  }
  return ctx;
}

// static
protos::pbzero::DebugAnnotation* TrackEventInternal::AddDebugAnnotation(
    perfetto::EventContext* event_ctx,
    const char* name) {
  auto annotation = event_ctx->event()->add_debug_annotations();
  annotation->set_name_iid(InternedDebugAnnotationName::Get(event_ctx, name));
  return annotation;
}

}  // namespace internal
}  // namespace perfetto

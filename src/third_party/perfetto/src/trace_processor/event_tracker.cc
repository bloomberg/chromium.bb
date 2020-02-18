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

#include "src/trace_processor/event_tracker.h"

#include <math.h>

#include "perfetto/ext/base/utils.h"
#include "src/trace_processor/args_tracker.h"
#include "src/trace_processor/ftrace_descriptors.h"
#include "src/trace_processor/ftrace_utils.h"
#include "src/trace_processor/process_tracker.h"
#include "src/trace_processor/stats.h"
#include "src/trace_processor/trace_processor_context.h"
#include "src/trace_processor/variadic.h"

#include "protos/perfetto/trace/ftrace/ftrace_event.pbzero.h"
#include "protos/perfetto/trace/ftrace/sched.pbzero.h"

namespace perfetto {
namespace trace_processor {

EventTracker::EventTracker(TraceProcessorContext* context) : context_(context) {
  auto* descriptor = GetMessageDescriptorForId(
      protos::pbzero::FtraceEvent::kSchedSwitchFieldNumber);
  PERFETTO_CHECK(descriptor->max_field_id == kSchedSwitchMaxFieldId);

  for (size_t i = 1; i <= kSchedSwitchMaxFieldId; i++) {
    sched_switch_field_ids_[i] =
        context->storage->InternString(descriptor->fields[i].name);
  }
  sched_switch_id_ = context->storage->InternString(descriptor->name);
}

EventTracker::~EventTracker() = default;

void EventTracker::PushSchedSwitch(uint32_t cpu,
                                   int64_t ts,
                                   uint32_t prev_pid,
                                   base::StringView prev_comm,
                                   int32_t prev_prio,
                                   int64_t prev_state,
                                   uint32_t next_pid,
                                   base::StringView next_comm,
                                   int32_t next_prio) {
  // At this stage all events should be globally timestamp ordered.
  if (ts < prev_timestamp_) {
    PERFETTO_ELOG("sched_switch event out of order by %.4f ms, skipping",
                  (prev_timestamp_ - ts) / 1e6);
    context_->storage->IncrementStats(stats::sched_switch_out_of_order);
    return;
  }
  prev_timestamp_ = ts;
  PERFETTO_DCHECK(cpu < base::kMaxCpus);

  auto* slices = context_->storage->mutable_slices();

  StringId next_comm_id = context_->storage->InternString(next_comm);
  auto next_utid =
      context_->process_tracker->UpdateThreadName(next_pid, next_comm_id);

  // First use this data to close the previous slice.
  bool prev_pid_match_prev_next_pid = false;
  auto* prev_slice = &pending_sched_per_cpu_[cpu];
  size_t slice_idx = prev_slice->storage_index;
  if (slice_idx < std::numeric_limits<size_t>::max()) {
    prev_pid_match_prev_next_pid = prev_pid == prev_slice->next_pid;
    if (PERFETTO_LIKELY(prev_pid_match_prev_next_pid)) {
      int64_t duration = ts - slices->start_ns()[slice_idx];
      slices->set_duration(slice_idx, duration);

      // We store the state as a uint16 as we only consider values up to 2048
      // when unpacking the information inside; this allows savings of 48 bits
      // per slice.
      auto task_state =
          ftrace_utils::TaskState(static_cast<uint16_t>(prev_state));
      if (!task_state.is_valid()) {
        context_->storage->IncrementStats(stats::task_state_invalid);
      }
      slices->set_end_state(slice_idx, task_state);
    } else {
      // If the pids ae not consistent, make a note of this.
      context_->storage->IncrementStats(stats::mismatched_sched_switch_tids);
    }
  }

  // We have to intern prev_comm again because our assumption that
  // this event's |prev_comm| == previous event's |next_comm| does not hold
  // if the thread changed its name while scheduled.
  StringId prev_comm_id = context_->storage->InternString(prev_comm);
  UniqueTid prev_utid =
      context_->process_tracker->UpdateThreadName(prev_pid, prev_comm_id);

  // Push the raw event - this is done as the raw ftrace event codepath does
  // not insert sched_switch.
  auto rid = context_->storage->mutable_raw_events()->AddRawEvent(
      ts, sched_switch_id_, cpu, prev_utid);

  // Note: this ordering is important. The events should be pushed in the same
  // order as the order of fields in the proto; this is used by the raw table to
  // index these events using the field ids.
  using SS = protos::pbzero::SchedSwitchFtraceEvent;
  auto add_raw_arg = [this](RowId row_id, int field_num, Variadic var) {
    StringId key = sched_switch_field_ids_[static_cast<size_t>(field_num)];
    context_->args_tracker->AddArg(row_id, key, key, var);
  };
  add_raw_arg(rid, SS::kPrevCommFieldNumber, Variadic::String(prev_comm_id));
  add_raw_arg(rid, SS::kPrevPidFieldNumber, Variadic::Integer(prev_pid));
  add_raw_arg(rid, SS::kPrevPrioFieldNumber, Variadic::Integer(prev_prio));
  add_raw_arg(rid, SS::kPrevStateFieldNumber, Variadic::Integer(prev_state));
  add_raw_arg(rid, SS::kNextCommFieldNumber, Variadic::String(next_comm_id));
  add_raw_arg(rid, SS::kNextPidFieldNumber, Variadic::Integer(next_pid));
  add_raw_arg(rid, SS::kNextPrioFieldNumber, Variadic::Integer(next_prio));

  // Add the slice for the "next" slice.
  auto next_idx = slices->AddSlice(cpu, ts, 0 /* duration */, next_utid,
                                   ftrace_utils::TaskState(), next_prio);

  // Finally, update the info for the next sched switch on this CPU.
  prev_slice->storage_index = next_idx;
  prev_slice->next_pid = next_pid;
}

RowId EventTracker::PushCounter(int64_t timestamp,
                                double value,
                                StringId name_id,
                                int64_t ref,
                                RefType ref_type,
                                bool resolve_utid_to_upid) {
  PERFETTO_DCHECK(!resolve_utid_to_upid || ref_type == RefType::kRefUtid);

  auto* definitions = context_->storage->mutable_counter_definitions();
  TraceStorage::CounterDefinitions::Id defn_id;
  if (resolve_utid_to_upid) {
    defn_id = TraceStorage::CounterDefinitions::kInvalidId;
  } else {
    defn_id = definitions->AddCounterDefinition(name_id, ref, ref_type);
  }
  RowId row_id = PushCounter(timestamp, value, defn_id);
  if (resolve_utid_to_upid) {
    auto table_and_row = TraceStorage::ParseRowId(row_id);
    PendingUpidResolutionCounter pending;
    pending.row = table_and_row.second;
    pending.utid = static_cast<UniqueTid>(ref);
    pending.name_id = name_id;
    pending_upid_resolution_counter_.emplace_back(pending);
  }
  return row_id;
}

RowId EventTracker::PushCounter(int64_t timestamp,
                                double value,
                                TraceStorage::CounterDefinitions::Id defn_id) {
  if (timestamp < prev_timestamp_) {
    PERFETTO_DLOG("counter event (ts: %" PRId64
                  ") out of order by %.4f ms, skipping",
                  timestamp, (prev_timestamp_ - timestamp) / 1e6);
    context_->storage->IncrementStats(stats::counter_events_out_of_order);
    return kInvalidRowId;
  }
  prev_timestamp_ = timestamp;

  auto* counter_values = context_->storage->mutable_counter_values();
  uint32_t idx = counter_values->AddCounterValue(defn_id, timestamp, value);
  return TraceStorage::CreateRowId(TableId::kCounterValues,
                                   static_cast<uint32_t>(idx));
}

RowId EventTracker::PushInstant(int64_t timestamp,
                                StringId name_id,
                                double value,
                                int64_t ref,
                                RefType ref_type,
                                bool resolve_utid_to_upid) {
  auto* instants = context_->storage->mutable_instants();
  uint32_t idx;
  if (resolve_utid_to_upid) {
    idx = instants->AddInstantEvent(timestamp, name_id, value, 0,
                                    RefType::kRefUpid);
    PendingUpidResolutionInstant pending;
    pending.row = idx;
    pending.utid = static_cast<UniqueTid>(ref);
    pending_upid_resolution_instant_.emplace_back(pending);
  } else {
    idx = instants->AddInstantEvent(timestamp, name_id, value, ref, ref_type);
  }
  return TraceStorage::CreateRowId(TableId::kInstants,
                                   static_cast<uint32_t>(idx));
}

void EventTracker::FlushPendingEvents() {
  // TODO(lalitm): the day this method is called before end of trace, don't
  // flush the sched events as they will probably be pushed in the next round
  // of ftrace events.
  int64_t end_ts = context_->storage->GetTraceTimestampBoundsNs().second;
  auto* slices = context_->storage->mutable_slices();
  for (const auto& pending_sched : pending_sched_per_cpu_) {
    size_t row = pending_sched.storage_index;
    if (row == std::numeric_limits<size_t>::max())
      continue;

    int64_t duration = end_ts - slices->start_ns()[row];
    slices->set_duration(row, duration);
    slices->set_end_state(
        row, ftrace_utils::TaskState(ftrace_utils::TaskState::kRunnable));
  }

  for (const auto& pending_counter : pending_upid_resolution_counter_) {
    const auto& thread = context_->storage->GetThread(pending_counter.utid);
    // TODO(lalitm): having upid == 0 is probably not the correct approach here
    // but it's unclear what may be better.
    UniquePid upid = thread.upid.value_or(0);
    auto id =
        context_->storage->mutable_counter_definitions()->AddCounterDefinition(
            pending_counter.name_id, upid, RefType::kRefUpid);
    context_->storage->mutable_counter_values()->set_counter_id(
        pending_counter.row, id);
  }

  for (const auto& pending_instant : pending_upid_resolution_instant_) {
    const auto& thread = context_->storage->GetThread(pending_instant.utid);
    // TODO(lalitm): having upid == 0 is probably not the correct approach here
    // but it's unclear what may be better.
    UniquePid upid = thread.upid.value_or(0);
    context_->storage->mutable_instants()->set_ref(pending_instant.row, upid);
  }

  pending_sched_per_cpu_ = {};
  pending_upid_resolution_counter_.clear();
  pending_upid_resolution_instant_.clear();
}

}  // namespace trace_processor
}  // namespace perfetto

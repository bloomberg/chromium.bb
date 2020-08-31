// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/common/frame_sinks/begin_frame_args.h"

#include <utility>

#include "base/trace_event/traced_value.h"
#include "third_party/perfetto/protos/perfetto/trace/track_event/chrome_compositor_scheduler_state.pbzero.h"
#include "third_party/perfetto/protos/perfetto/trace/track_event/source_location.pbzero.h"

namespace viz {

const char* BeginFrameArgs::TypeToString(BeginFrameArgsType type) {
  switch (type) {
    case BeginFrameArgs::INVALID:
      return "INVALID";
    case BeginFrameArgs::NORMAL:
      return "NORMAL";
    case BeginFrameArgs::MISSED:
      return "MISSED";
  }
  NOTREACHED();
  return "???";
}

namespace {
perfetto::protos::pbzero::BeginFrameArgs::BeginFrameArgsType
TypeToProtozeroEnum(BeginFrameArgs::BeginFrameArgsType type) {
  using pbzeroType = perfetto::protos::pbzero::BeginFrameArgs;
  switch (type) {
    case BeginFrameArgs::INVALID:
      return pbzeroType::BEGIN_FRAME_ARGS_TYPE_INVALID;
    case BeginFrameArgs::NORMAL:
      return pbzeroType::BEGIN_FRAME_ARGS_TYPE_NORMAL;
    case BeginFrameArgs::MISSED:
      return pbzeroType::BEGIN_FRAME_ARGS_TYPE_MISSED;
  }
  NOTREACHED();
  return pbzeroType::BEGIN_FRAME_ARGS_TYPE_UNSPECIFIED;
}
}  // namespace

constexpr uint64_t BeginFrameArgs::kInvalidFrameNumber;
constexpr uint64_t BeginFrameArgs::kStartingFrameNumber;

BeginFrameId::BeginFrameId()
    : source_id(0), sequence_number(BeginFrameArgs::kInvalidFrameNumber) {}

BeginFrameId::BeginFrameId(const BeginFrameId& id) = default;
BeginFrameId& BeginFrameId::operator=(const BeginFrameId& id) = default;

BeginFrameId::BeginFrameId(uint64_t source_id, uint64_t sequence_number)
    : source_id(source_id), sequence_number(sequence_number) {}

bool BeginFrameId::operator<(const BeginFrameId& other) const {
  if (source_id == other.source_id)
    return (sequence_number < other.sequence_number);
  return (source_id < other.source_id);
}

bool BeginFrameId::operator==(const BeginFrameId& other) const {
  return (source_id == other.source_id &&
          sequence_number == other.sequence_number);
}
bool BeginFrameId::operator!=(const BeginFrameId& other) const {
  return !(*this == other);
}

bool BeginFrameId::IsNextInSequenceTo(const BeginFrameId& previous) const {
  return (source_id == previous.source_id &&
          sequence_number > previous.sequence_number);
}

bool BeginFrameId::IsSequenceValid() const {
  return (BeginFrameArgs::kInvalidFrameNumber != sequence_number);
}

std::string BeginFrameId::ToString() const {
  base::trace_event::TracedValueJSON value;
  value.SetInteger("source_id", source_id);
  value.SetInteger("sequence_number", sequence_number);
  return value.ToJSON();
}

BeginFrameArgs::BeginFrameArgs()
    : frame_time(base::TimeTicks::Min()),
      deadline(base::TimeTicks::Min()),
      interval(base::TimeDelta::FromMicroseconds(-1)),
      frame_id(BeginFrameId(0, kInvalidFrameNumber)),
      type(BeginFrameArgs::INVALID),
      on_critical_path(true),
      animate_only(false) {}

BeginFrameArgs::BeginFrameArgs(uint64_t source_id,
                               uint64_t sequence_number,
                               base::TimeTicks frame_time,
                               base::TimeTicks deadline,
                               base::TimeDelta interval,
                               BeginFrameArgs::BeginFrameArgsType type)
    : frame_time(frame_time),
      deadline(deadline),
      interval(interval),
      frame_id(BeginFrameId(source_id, sequence_number)),
      type(type),
      on_critical_path(true),
      animate_only(false) {
  DCHECK_LE(kStartingFrameNumber, sequence_number);
}

BeginFrameArgs::BeginFrameArgs(const BeginFrameArgs& args) = default;
BeginFrameArgs& BeginFrameArgs::operator=(const BeginFrameArgs& args) = default;

BeginFrameArgs BeginFrameArgs::Create(BeginFrameArgs::CreationLocation location,
                                      uint64_t source_id,
                                      uint64_t sequence_number,
                                      base::TimeTicks frame_time,
                                      base::TimeTicks deadline,
                                      base::TimeDelta interval,
                                      BeginFrameArgs::BeginFrameArgsType type) {
  DCHECK_NE(type, BeginFrameArgs::INVALID);
#ifdef NDEBUG
  return BeginFrameArgs(source_id, sequence_number, frame_time, deadline,
                        interval, type);
#else
  BeginFrameArgs args = BeginFrameArgs(source_id, sequence_number, frame_time,
                                       deadline, interval, type);
  args.created_from = location;
  return args;
#endif
}

std::unique_ptr<base::trace_event::ConvertableToTraceFormat>
BeginFrameArgs::AsValue() const {
  std::unique_ptr<base::trace_event::TracedValue> state(
      new base::trace_event::TracedValue());
  AsValueInto(state.get());
  return std::move(state);
}

void BeginFrameArgs::AsValueInto(base::trace_event::TracedValue* state) const {
  state->SetString("type", "BeginFrameArgs");
  state->SetString("subtype", TypeToString(type));
  state->SetInteger("source_id", frame_id.source_id);
  state->SetInteger("sequence_number", frame_id.sequence_number);
  state->SetDouble("frame_time_us", frame_time.since_origin().InMicroseconds());
  state->SetDouble("deadline_us", deadline.since_origin().InMicroseconds());
  state->SetDouble("interval_us", interval.InMicroseconds());
#ifndef NDEBUG
  state->SetString("created_from", created_from.ToString());
#endif
  state->SetBoolean("on_critical_path", on_critical_path);
  state->SetBoolean("animate_only", animate_only);
}

void BeginFrameArgs::AsProtozeroInto(
    perfetto::protos::pbzero::BeginFrameArgs* state) const {
  state->set_type(TypeToProtozeroEnum(type));
  state->set_source_id(frame_id.source_id);
  state->set_sequence_number(frame_id.sequence_number);
  state->set_frame_time_us(frame_time.since_origin().InMicroseconds());
  state->set_deadline_us(deadline.since_origin().InMicroseconds());
  state->set_interval_delta_us(interval.InMicroseconds());
  state->set_on_critical_path(on_critical_path);
  state->set_animate_only(animate_only);
#ifndef NDEBUG
  auto* src_loc = state->set_source_location();
  if (created_from.file_name()) {
    src_loc->set_file_name(created_from.file_name());
  }
  if (created_from.function_name()) {
    src_loc->set_function_name(created_from.function_name());
  }
  if (created_from.line_number() != -1) {
    src_loc->set_line_number(created_from.line_number());
  }
#endif
}

std::string BeginFrameArgs::ToString() const {
  base::trace_event::TracedValueJSON value;
  AsValueInto(&value);
  return value.ToJSON();
}

BeginFrameAck::BeginFrameAck() : has_damage(false) {}

BeginFrameAck::BeginFrameAck(const BeginFrameArgs& args, bool has_damage)
    : BeginFrameAck(args.frame_id.source_id,
                    args.frame_id.sequence_number,
                    has_damage,
                    args.trace_id) {}

BeginFrameAck::BeginFrameAck(uint64_t source_id,
                             uint64_t sequence_number,
                             bool has_damage,
                             int64_t trace_id)
    : frame_id(BeginFrameId(source_id, sequence_number)),
      trace_id(trace_id),
      has_damage(has_damage) {
  DCHECK(frame_id.IsSequenceValid());
}

// static
BeginFrameAck BeginFrameAck::CreateManualAckWithDamage() {
  return BeginFrameAck(BeginFrameArgs::kManualSourceId,
                       BeginFrameArgs::kStartingFrameNumber, true);
}

}  // namespace viz

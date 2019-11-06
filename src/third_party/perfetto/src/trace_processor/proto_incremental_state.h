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

#ifndef SRC_TRACE_PROCESSOR_PROTO_INCREMENTAL_STATE_H_
#define SRC_TRACE_PROCESSOR_PROTO_INCREMENTAL_STATE_H_

#include <stdint.h>

#include <map>
#include <unordered_map>

#include "perfetto/ext/base/optional.h"
#include "perfetto/protozero/proto_decoder.h"
#include "src/trace_processor/trace_blob_view.h"
#include "src/trace_processor/trace_storage.h"

#include "protos/perfetto/trace/profiling/profile_common.pbzero.h"
#include "protos/perfetto/trace/track_event/debug_annotation.pbzero.h"
#include "protos/perfetto/trace/track_event/log_message.pbzero.h"
#include "protos/perfetto/trace/track_event/source_location.pbzero.h"
#include "protos/perfetto/trace/track_event/task_execution.pbzero.h"
#include "protos/perfetto/trace/track_event/track_event.pbzero.h"

namespace perfetto {
namespace trace_processor {

// Specialization of member types is forbidden inside their parent class, so
// define the StorageReferences class outside in an internal namespace.
namespace proto_incremental_state_internal {

template <typename MessageType>
struct StorageReferences;

template <>
struct StorageReferences<protos::pbzero::InternedString> {};
template <>
struct StorageReferences<protos::pbzero::Mapping> {};
template <>
struct StorageReferences<protos::pbzero::Frame> {};
template <>
struct StorageReferences<protos::pbzero::Callstack> {};

template <>
struct StorageReferences<protos::pbzero::EventCategory> {
  StringId name_id;
};

template <>
struct StorageReferences<protos::pbzero::EventName> {
  StringId name_id;
};

template <>
struct StorageReferences<protos::pbzero::DebugAnnotationName> {
  StringId name_id;
};

template <>
struct StorageReferences<protos::pbzero::SourceLocation> {
  StringId file_name_id;
  StringId function_name_id;
  uint32_t line_number;
};

template <>
struct StorageReferences<protos::pbzero::LogMessageBody> {
  StringId body_id;
};

}  // namespace proto_incremental_state_internal

// Stores per-packet-sequence incremental state during trace parsing, such as
// reference timestamps for delta timestamp calculation and interned messages.
class ProtoIncrementalState {
 public:
  template <typename MessageType>
  using StorageReferences =
      proto_incremental_state_internal::StorageReferences<MessageType>;

  // Entry in an interning index, refers to the interned message.
  template <typename MessageType>
  struct InternedDataView {
    InternedDataView(TraceBlobView msg) : message(std::move(msg)) {}

    typename MessageType::Decoder CreateDecoder() const {
      return typename MessageType::Decoder(message.data(), message.length());
    }

    TraceBlobView message;

    // If the data in this entry was already stored into the trace storage, this
    // field contains message-type-specific references into the storage which
    // can be used to look up the entry's data (e.g. indexes of interned
    // strings).
    base::Optional<StorageReferences<MessageType>> storage_refs;
  };

  template <typename MessageType>
  using InternedDataMap =
      std::unordered_map<uint64_t, InternedDataView<MessageType>>;

  class PacketSequenceState {
   public:
    int64_t IncrementAndGetTrackEventTimeNs(int64_t delta_ns) {
      PERFETTO_DCHECK(IsTrackEventStateValid());
      track_event_timestamp_ns_ += delta_ns;
      return track_event_timestamp_ns_;
    }

    int64_t IncrementAndGetTrackEventThreadTimeNs(int64_t delta_ns) {
      PERFETTO_DCHECK(IsTrackEventStateValid());
      track_event_thread_timestamp_ns_ += delta_ns;
      return track_event_thread_timestamp_ns_;
    }

    int64_t IncrementAndGetTrackEventThreadInstructionCount(int64_t delta) {
      PERFETTO_DCHECK(IsTrackEventStateValid());
      track_event_thread_instruction_count_ += delta;
      return track_event_thread_instruction_count_;
    }

    void OnPacketLoss() {
      packet_loss_ = true;
      thread_descriptor_seen_ = false;
    }

    void OnIncrementalStateCleared() { packet_loss_ = false; }

    void SetThreadDescriptor(int32_t pid,
                             int32_t tid,
                             int64_t timestamp_ns,
                             int64_t thread_timestamp_ns,
                             int64_t thread_instruction_count) {
      thread_descriptor_seen_ = true;
      pid_ = pid;
      tid_ = tid;
      track_event_timestamp_ns_ = timestamp_ns;
      track_event_thread_timestamp_ns_ = thread_timestamp_ns;
      track_event_thread_instruction_count_ = thread_instruction_count;
    }

    bool IsIncrementalStateValid() const { return !packet_loss_; }

    bool IsTrackEventStateValid() const {
      return IsIncrementalStateValid() && thread_descriptor_seen_;
    }

    int32_t pid() const { return pid_; }
    int32_t tid() const { return tid_; }

    template <typename MessageType>
    InternedDataMap<MessageType>* GetInternedDataMap();

   private:
    // If true, incremental state on the sequence is considered invalid until we
    // see the next packet with incremental_state_cleared. We assume that we
    // missed some packets at the beginning of the trace.
    bool packet_loss_ = true;

    // We can only consider TrackEvent delta timestamps to be correct after we
    // have observed a thread descriptor (since the last packet loss).
    bool thread_descriptor_seen_ = false;

    // Process/thread ID of the packet sequence. Used as default values for
    // TrackEvents that don't specify a pid/tid override. Only valid while
    // |seen_thread_descriptor_| is true.
    int32_t pid_ = 0;
    int32_t tid_ = 0;

    // Current wall/thread timestamps/counters used as reference for the next
    // TrackEvent delta timestamp.
    int64_t track_event_timestamp_ns_ = 0;
    int64_t track_event_thread_timestamp_ns_ = 0;
    int64_t track_event_thread_instruction_count_ = 0;

    InternedDataMap<protos::pbzero::EventCategory> event_categories_;
    InternedDataMap<protos::pbzero::EventName> event_names_;
    InternedDataMap<protos::pbzero::DebugAnnotationName>
        debug_annotation_names_;
    InternedDataMap<protos::pbzero::SourceLocation> source_locations_;
    InternedDataMap<protos::pbzero::InternedString> interned_strings_;
    InternedDataMap<protos::pbzero::LogMessageBody> interned_log_messages_;
    InternedDataMap<protos::pbzero::Mapping> mappings_;
    InternedDataMap<protos::pbzero::Frame> frames_;
    InternedDataMap<protos::pbzero::Callstack> callstacks_;
  };

  // Returns the PacketSequenceState for the packet sequence with the given id.
  // If this is a new sequence which we haven't tracked before, initializes and
  // inserts a new PacketSequenceState into the state map.
  PacketSequenceState* GetOrCreateStateForPacketSequence(uint32_t sequence_id) {
    auto& ptr = packet_sequence_states_[sequence_id];
    if (!ptr)
      ptr.reset(new PacketSequenceState());
    return ptr.get();
  }

 private:
  // Stores unique_ptrs to ensure that pointers to a PacketSequenceState remain
  // valid even if the map rehashes.
  std::map<uint32_t, std::unique_ptr<PacketSequenceState>>
      packet_sequence_states_;
};

template <>
inline ProtoIncrementalState::InternedDataMap<protos::pbzero::EventCategory>*
ProtoIncrementalState::PacketSequenceState::GetInternedDataMap<
    protos::pbzero::EventCategory>() {
  return &event_categories_;
}

template <>
inline ProtoIncrementalState::InternedDataMap<protos::pbzero::EventName>*
ProtoIncrementalState::PacketSequenceState::GetInternedDataMap<
    protos::pbzero::EventName>() {
  return &event_names_;
}

template <>
inline ProtoIncrementalState::InternedDataMap<
    protos::pbzero::DebugAnnotationName>*
ProtoIncrementalState::PacketSequenceState::GetInternedDataMap<
    protos::pbzero::DebugAnnotationName>() {
  return &debug_annotation_names_;
}

template <>
inline ProtoIncrementalState::InternedDataMap<protos::pbzero::SourceLocation>*
ProtoIncrementalState::PacketSequenceState::GetInternedDataMap<
    protos::pbzero::SourceLocation>() {
  return &source_locations_;
}

template <>
inline ProtoIncrementalState::InternedDataMap<protos::pbzero::LogMessageBody>*
ProtoIncrementalState::PacketSequenceState::GetInternedDataMap<
    protos::pbzero::LogMessageBody>() {
  return &interned_log_messages_;
}

template <>
inline ProtoIncrementalState::InternedDataMap<protos::pbzero::InternedString>*
ProtoIncrementalState::PacketSequenceState::GetInternedDataMap<
    protos::pbzero::InternedString>() {
  return &interned_strings_;
}

template <>
inline ProtoIncrementalState::InternedDataMap<protos::pbzero::Mapping>*
ProtoIncrementalState::PacketSequenceState::GetInternedDataMap<
    protos::pbzero::Mapping>() {
  return &mappings_;
}

template <>
inline ProtoIncrementalState::InternedDataMap<protos::pbzero::Frame>*
ProtoIncrementalState::PacketSequenceState::GetInternedDataMap<
    protos::pbzero::Frame>() {
  return &frames_;
}

template <>
inline ProtoIncrementalState::InternedDataMap<protos::pbzero::Callstack>*
ProtoIncrementalState::PacketSequenceState::GetInternedDataMap<
    protos::pbzero::Callstack>() {
  return &callstacks_;
}
}  // namespace trace_processor
}  // namespace perfetto

#endif  // SRC_TRACE_PROCESSOR_PROTO_INCREMENTAL_STATE_H_

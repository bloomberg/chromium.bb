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

#include "src/profiling/memory/bookkeeping_dump.h"

namespace perfetto {
namespace profiling {
namespace {
using ::perfetto::protos::pbzero::ProfilePacket;
// This needs to be lower than the maximum acceptable chunk size, because this
// is checked *before* writing another submessage. We conservatively assume
// submessages can be up to 100k here for a 500k chunk size.
// DropBox has a 500k chunk limit, and each chunk needs to parse as a proto.
uint32_t kPacketSizeThreshold = 400000;
}  // namespace

void DumpState::WriteMap(const Interned<Mapping> map) {
  auto map_it_and_inserted = dumped_mappings_.emplace(map.id());
  if (map_it_and_inserted.second) {
    for (const Interned<std::string>& str : map->path_components)
      WriteString(str);

    WriteString(map->build_id);

    if (currently_written() > kPacketSizeThreshold)
      NewProfilePacket();

    auto mapping = current_profile_packet_->add_mappings();
    mapping->set_id(map.id());
    mapping->set_offset(map->offset);
    mapping->set_start(map->start);
    mapping->set_end(map->end);
    mapping->set_load_bias(map->load_bias);
    mapping->set_build_id(map->build_id.id());
    for (const Interned<std::string>& str : map->path_components)
      mapping->add_path_string_ids(str.id());
  }
}

void DumpState::WriteFrame(Interned<Frame> frame) {
  WriteMap(frame->mapping);
  WriteString(frame->function_name);
  bool inserted;
  std::tie(std::ignore, inserted) = dumped_frames_.emplace(frame.id());
  if (inserted) {
    if (currently_written() > kPacketSizeThreshold)
      NewProfilePacket();

    auto frame_proto = current_profile_packet_->add_frames();
    frame_proto->set_id(frame.id());
    frame_proto->set_function_name_id(frame->function_name.id());
    frame_proto->set_mapping_id(frame->mapping.id());
    frame_proto->set_rel_pc(frame->rel_pc);
  }
}

void DumpState::WriteString(const Interned<std::string>& str) {
  bool inserted;
  std::tie(std::ignore, inserted) = dumped_strings_.emplace(str.id());
  if (inserted) {
    if (currently_written() > kPacketSizeThreshold)
      NewProfilePacket();

    auto interned_string = current_profile_packet_->add_strings();
    interned_string->set_id(str.id());
    interned_string->set_str(reinterpret_cast<const uint8_t*>(str->c_str()),
                             str->size());
  }
}

void DumpState::StartProcessDump(
    std::function<void(protos::pbzero::ProfilePacket::ProcessHeapSamples*)>
        fill_process_header) {
  current_process_fill_header_ = std::move(fill_process_header);
  current_process_heap_samples_ = nullptr;
  current_process_idle_allocs_.clear();
}

void DumpState::WriteAllocation(
    const HeapTracker::CallstackAllocations& alloc) {
  callstacks_to_dump_.emplace(alloc.node);
  auto* heap_samples = GetCurrentProcessHeapSamples();
  ProfilePacket::HeapSample* sample = heap_samples->add_samples();
  sample->set_callstack_id(alloc.node->id());
  sample->set_self_allocated(alloc.allocated);
  sample->set_self_freed(alloc.freed);
  sample->set_alloc_count(alloc.allocation_count);
  sample->set_free_count(alloc.free_count);

  auto it = current_process_idle_allocs_.find(alloc.node->id());
  if (it != current_process_idle_allocs_.end())
    sample->set_self_idle(it->second);
}

void DumpState::DumpCallstacks(GlobalCallstackTrie* callsites) {
  for (GlobalCallstackTrie::Node* node : callstacks_to_dump_) {
    // There need to be two separate loops over built_callstack because
    // protozero cannot interleave different messages.
    auto built_callstack = callsites->BuildCallstack(node);
    for (const Interned<Frame>& frame : built_callstack)
      WriteFrame(frame);
    ProfilePacket::Callstack* callstack =
        current_profile_packet_->add_callstacks();
    callstack->set_id(node->id());
    for (const Interned<Frame>& frame : built_callstack)
      callstack->add_frame_ids(frame.id());
  }
}

void DumpState::AddIdleBytes(uintptr_t callstack_id, uint64_t bytes) {
  current_process_idle_allocs_[callstack_id] += bytes;
}

void DumpState::RejectConcurrent(pid_t pid) {
  ProfilePacket::ProcessHeapSamples* proto =
      current_profile_packet_->add_process_dumps();
  proto->set_pid(static_cast<uint64_t>(pid));
  proto->set_rejected_concurrent(true);
}

ProfilePacket::ProcessHeapSamples* DumpState::GetCurrentProcessHeapSamples() {
  if (currently_written() > kPacketSizeThreshold &&
      current_process_heap_samples_ != nullptr) {
    NewProfilePacket();
    current_process_heap_samples_ = nullptr;
  }

  if (current_process_heap_samples_ == nullptr) {
    current_process_heap_samples_ =
        current_profile_packet_->add_process_dumps();
    current_process_fill_header_(current_process_heap_samples_);
  }

  return current_process_heap_samples_;
}

}  // namespace profiling
}  // namespace perfetto

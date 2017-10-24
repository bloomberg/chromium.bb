// Copyright 2014 The Crashpad Authors. All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "minidump/minidump_memory_writer.h"

#include <utility>

#include "base/auto_reset.h"
#include "base/logging.h"
#include "util/file/file_writer.h"
#include "util/numeric/safe_assignment.h"

namespace crashpad {

SnapshotMinidumpMemoryWriter::SnapshotMinidumpMemoryWriter(
    const MemorySnapshot* memory_snapshot)
    : internal::MinidumpWritable(),
      MemorySnapshot::Delegate(),
      memory_descriptor_(),
      registered_memory_descriptors_(),
      memory_snapshot_(memory_snapshot),
      file_writer_(nullptr) {}

SnapshotMinidumpMemoryWriter::~SnapshotMinidumpMemoryWriter() {}

bool SnapshotMinidumpMemoryWriter::MemorySnapshotDelegateRead(void* data,
                                                              size_t size) {
  DCHECK_EQ(state(), kStateWritable);
  DCHECK_EQ(size, UnderlyingSnapshot().Size());
  return file_writer_->Write(data, size);
}

bool SnapshotMinidumpMemoryWriter::WriteObject(
    FileWriterInterface* file_writer) {
  DCHECK_EQ(state(), kStateWritable);
  DCHECK(!file_writer_);

  base::AutoReset<FileWriterInterface*> file_writer_reset(&file_writer_,
                                                          file_writer);

  // This will result in MemorySnapshotDelegateRead() being called.
  return memory_snapshot_->Read(this);
}

const MINIDUMP_MEMORY_DESCRIPTOR*
SnapshotMinidumpMemoryWriter::MinidumpMemoryDescriptor() const {
  DCHECK_EQ(state(), kStateWritable);

  return &memory_descriptor_;
}

void SnapshotMinidumpMemoryWriter::RegisterMemoryDescriptor(
    MINIDUMP_MEMORY_DESCRIPTOR* memory_descriptor) {
  DCHECK_LE(state(), kStateFrozen);

  registered_memory_descriptors_.push_back(memory_descriptor);
  RegisterLocationDescriptor(&memory_descriptor->Memory);
}

bool SnapshotMinidumpMemoryWriter::Freeze() {
  DCHECK_EQ(state(), kStateMutable);

  if (!MinidumpWritable::Freeze()) {
    return false;
  }

  RegisterMemoryDescriptor(&memory_descriptor_);

  return true;
}

size_t SnapshotMinidumpMemoryWriter::Alignment() {
  DCHECK_GE(state(), kStateFrozen);

  return 16;
}

size_t SnapshotMinidumpMemoryWriter::SizeOfObject() {
  DCHECK_GE(state(), kStateFrozen);

  return UnderlyingSnapshot().Size();
}

bool SnapshotMinidumpMemoryWriter::WillWriteAtOffsetImpl(FileOffset offset) {
  DCHECK_EQ(state(), kStateFrozen);

  // There will always be at least one registered descriptor, the one for this
  // object’s own memory_descriptor_ field.
  DCHECK_GE(registered_memory_descriptors_.size(), 1u);

  uint64_t base_address = UnderlyingSnapshot().Address();
  decltype(registered_memory_descriptors_[0]->StartOfMemoryRange) local_address;
  if (!AssignIfInRange(&local_address, base_address)) {
    LOG(ERROR) << "base_address " << base_address << " out of range";
    return false;
  }

  for (MINIDUMP_MEMORY_DESCRIPTOR* memory_descriptor :
           registered_memory_descriptors_) {
    memory_descriptor->StartOfMemoryRange = local_address;
  }

  return MinidumpWritable::WillWriteAtOffsetImpl(offset);
}

internal::MinidumpWritable::Phase SnapshotMinidumpMemoryWriter::WritePhase() {
  // Memory dumps are large and are unlikely to be consumed in their entirety.
  // Data accesses are expected to be sparse and sporadic, and are expected to
  // occur after all of the other structural and informational data from the
  // minidump file has been read. Put memory dumps at the end of the minidump
  // file to improve spatial locality.
  return kPhaseLate;
}

MinidumpMemoryListWriter::MinidumpMemoryListWriter()
    : MinidumpStreamWriter(),
      memory_writers_(),
      children_(),
      memory_list_base_() {
}

MinidumpMemoryListWriter::~MinidumpMemoryListWriter() {
}

void MinidumpMemoryListWriter::AddFromSnapshot(
    const std::vector<const MemorySnapshot*>& memory_snapshots) {
  DCHECK_EQ(state(), kStateMutable);

  for (const MemorySnapshot* memory_snapshot : memory_snapshots) {
    std::unique_ptr<SnapshotMinidumpMemoryWriter> memory(
        new SnapshotMinidumpMemoryWriter(memory_snapshot));
    AddMemory(std::move(memory));
  }
}

void MinidumpMemoryListWriter::AddMemory(
    std::unique_ptr<SnapshotMinidumpMemoryWriter> memory_writer) {
  DCHECK_EQ(state(), kStateMutable);

  AddExtraMemory(memory_writer.get());
  children_.push_back(std::move(memory_writer));
}

void MinidumpMemoryListWriter::AddExtraMemory(
    SnapshotMinidumpMemoryWriter* memory_writer) {
  DCHECK_EQ(state(), kStateMutable);

  memory_writers_.push_back(memory_writer);
}

bool MinidumpMemoryListWriter::Freeze() {
  DCHECK_EQ(state(), kStateMutable);

  if (!MinidumpStreamWriter::Freeze()) {
    return false;
  }

  size_t memory_region_count = memory_writers_.size();
  CHECK_LE(children_.size(), memory_region_count);

  if (!AssignIfInRange(&memory_list_base_.NumberOfMemoryRanges,
                       memory_region_count)) {
    LOG(ERROR) << "memory_region_count " << memory_region_count
               << " out of range";
    return false;
  }

  return true;
}

size_t MinidumpMemoryListWriter::SizeOfObject() {
  DCHECK_GE(state(), kStateFrozen);
  DCHECK_LE(children_.size(), memory_writers_.size());

  return sizeof(memory_list_base_) +
         memory_writers_.size() * sizeof(MINIDUMP_MEMORY_DESCRIPTOR);
}

std::vector<internal::MinidumpWritable*> MinidumpMemoryListWriter::Children() {
  DCHECK_GE(state(), kStateFrozen);
  DCHECK_LE(children_.size(), memory_writers_.size());

  std::vector<MinidumpWritable*> children;
  for (const auto& child : children_) {
    children.push_back(child.get());
  }

  return children;
}

bool MinidumpMemoryListWriter::WriteObject(FileWriterInterface* file_writer) {
  DCHECK_EQ(state(), kStateWritable);

  WritableIoVec iov;
  iov.iov_base = &memory_list_base_;
  iov.iov_len = sizeof(memory_list_base_);
  std::vector<WritableIoVec> iovecs(1, iov);

  for (const SnapshotMinidumpMemoryWriter* memory_writer : memory_writers_) {
    iov.iov_base = memory_writer->MinidumpMemoryDescriptor();
    iov.iov_len = sizeof(MINIDUMP_MEMORY_DESCRIPTOR);
    iovecs.push_back(iov);
  }

  return file_writer->WriteIoVec(&iovecs);
}

MinidumpStreamType MinidumpMemoryListWriter::StreamType() const {
  return kMinidumpStreamTypeMemoryList;
}

}  // namespace crashpad

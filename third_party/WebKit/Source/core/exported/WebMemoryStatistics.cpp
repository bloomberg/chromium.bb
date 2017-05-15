// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "public/web/WebMemoryStatistics.h"

#include "platform/heap/Handle.h"
#include "platform/wtf/allocator/Partitions.h"

namespace blink {

namespace {

class LightPartitionStatsDumperImpl : public WTF::PartitionStatsDumper {
 public:
  LightPartitionStatsDumperImpl() : total_active_bytes_(0) {}

  void PartitionDumpTotals(
      const char* partition_name,
      const WTF::PartitionMemoryStats* memory_stats) override {
    total_active_bytes_ += memory_stats->total_active_bytes;
  }

  void PartitionsDumpBucketStats(
      const char* partition_name,
      const WTF::PartitionBucketMemoryStats*) override {}

  size_t TotalActiveBytes() const { return total_active_bytes_; }

 private:
  size_t total_active_bytes_;
};

}  // namespace

WebMemoryStatistics WebMemoryStatistics::Get() {
  LightPartitionStatsDumperImpl dumper;
  WebMemoryStatistics statistics;

  WTF::Partitions::DumpMemoryStats(true, &dumper);
  statistics.partition_alloc_total_allocated_bytes = dumper.TotalActiveBytes();

  statistics.blink_gc_total_allocated_bytes =
      ProcessHeap::TotalAllocatedObjectSize() +
      ProcessHeap::TotalMarkedObjectSize();
  return statistics;
}

}  // namespace blink

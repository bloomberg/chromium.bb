// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_ALLOCATOR_PARTITION_ALLOCATOR_MEMORY_RECLAIMER_H_
#define BASE_ALLOCATOR_PARTITION_ALLOCATOR_MEMORY_RECLAIMER_H_

#include <memory>
#include <set>

#include "base/allocator/partition_allocator/partition_alloc_base/no_destructor.h"
#include "base/allocator/partition_allocator/partition_alloc_forward.h"
#include "base/allocator/partition_allocator/partition_lock.h"
#include "base/base_export.h"
#include "base/thread_annotations.h"
#include "base/time/time.h"

namespace partition_alloc {

// Posts and handles memory reclaim tasks for PartitionAlloc.
//
// Thread safety: |RegisterPartition()| and |UnregisterPartition()| can be
// called from any thread, concurrently with reclaim. Reclaim itself runs in the
// context of the provided |SequencedTaskRunner|, meaning that the caller must
// take care of this runner being compatible with the various partitions.
//
// Singleton as this runs as long as the process is alive, and
// having multiple instances would be wasteful.
class BASE_EXPORT MemoryReclaimer {
 public:
  static MemoryReclaimer* Instance();

  MemoryReclaimer(const MemoryReclaimer&) = delete;
  MemoryReclaimer& operator=(const MemoryReclaimer&) = delete;

  // Internal. Do not use.
  // Registers a partition to be tracked by the reclaimer.
  void RegisterPartition(PartitionRoot<>* partition);
  // Internal. Do not use.
  // Unregisters a partition to be tracked by the reclaimer.
  void UnregisterPartition(PartitionRoot<>* partition);

  // Triggers an explicit reclaim now to reclaim as much free memory as
  // possible. The API callers need to invoke this method periodically
  // if they want to use memory reclaimer.
  // See also GetRecommendedReclaimIntervalInMicroseconds()'s comment.
  void ReclaimNormal();

  // Returns a recommended interval to invoke ReclaimNormal.
  int64_t GetRecommendedReclaimIntervalInMicroseconds() {
    return base::Seconds(4).InMicroseconds();
  }

  // Triggers an explicit reclaim now reclaiming all free memory
  void ReclaimAll();

 private:
  MemoryReclaimer();
  ~MemoryReclaimer();
  // |flags| is an OR of base::PartitionPurgeFlags
  void Reclaim(int flags);
  void ReclaimAndReschedule();
  void ResetForTesting();

  internal::Lock lock_;
  std::set<PartitionRoot<>*> partitions_ GUARDED_BY(lock_);

  friend class internal::base::NoDestructor<MemoryReclaimer>;
  friend class MemoryReclaimerTest;
};

}  // namespace partition_alloc

namespace base {

// TODO(https://crbug.com/1288247): Remove these 'using' declarations once
// the migration to the new namespaces gets done.
using ::partition_alloc::MemoryReclaimer;

}  // namespace base

#endif  // BASE_ALLOCATOR_PARTITION_ALLOCATOR_MEMORY_RECLAIMER_H_

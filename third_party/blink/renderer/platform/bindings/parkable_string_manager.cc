// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/bindings/parkable_string_manager.h"

#include <utility>

#include "base/bind.h"
#include "base/macros.h"
#include "base/metrics/histogram_macros.h"
#include "base/single_thread_task_runner.h"
#include "base/trace_event/memory_allocator_dump.h"
#include "base/trace_event/process_memory_dump.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/platform/bindings/parkable_string.h"
#include "third_party/blink/renderer/platform/memory_coordinator.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread_scheduler.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"
#include "third_party/blink/renderer/platform/wtf/wtf.h"

namespace blink {

namespace {

class OnPurgeMemoryListener : public GarbageCollected<OnPurgeMemoryListener>,
                              public MemoryCoordinatorClient {
  USING_GARBAGE_COLLECTED_MIXIN(OnPurgeMemoryListener);

  void OnPurgeMemory() override {
    if (!base::FeatureList::IsEnabled(kCompressParkableStringsInBackground))
      return;
    ParkableStringManager::Instance().PurgeMemory();
  }
};

}  // namespace

// static
ParkableStringManagerDumpProvider*
ParkableStringManagerDumpProvider::Instance() {
  static ParkableStringManagerDumpProvider instance;
  return &instance;
}

bool ParkableStringManagerDumpProvider::OnMemoryDump(
    const base::trace_event::MemoryDumpArgs& args,
    base::trace_event::ProcessMemoryDump* pmd) {
  return ParkableStringManager::Instance().OnMemoryDump(pmd);
}

ParkableStringManagerDumpProvider::~ParkableStringManagerDumpProvider() =
    default;
ParkableStringManagerDumpProvider::ParkableStringManagerDumpProvider() =
    default;

ParkableStringManager& ParkableStringManager::Instance() {
  DCHECK(IsMainThread());
  DEFINE_STATIC_LOCAL(ParkableStringManager, instance, ());
  return instance;
}

ParkableStringManager::~ParkableStringManager() = default;

void ParkableStringManager::SetRendererBackgrounded(bool backgrounded) {
  DCHECK(IsMainThread());
  backgrounded_ = backgrounded;

  if (!base::FeatureList::IsEnabled(kCompressParkableStringsInBackground))
    return;

  if (backgrounded_) {
    scoped_refptr<base::SingleThreadTaskRunner> task_runner =
        Thread::Current()->GetTaskRunner();
    DCHECK(task_runner);
    task_runner->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&ParkableStringManager::ParkAllIfRendererBackgrounded,
                       base::Unretained(this),
                       ParkableStringImpl::ParkingMode::kAlways),
        base::TimeDelta::FromSeconds(kParkingDelayInSeconds));
    // We only want to record statistics in the following case: a foreground tab
    // goes to background, and stays in background until the stats are recorded,
    // to make analysis simpler.
    //
    // To that end:
    // 1. Don't post a recording task if one has been posted and hasn't run yet.
    // 2. Any background -> foreground transition between now and the
    //    recording task running cancels the task.
    //
    // Also drop strings that can be dropped cheaply in this task, to prevent
    // used-once strings from increasing memory usage.
    if (!waiting_to_record_stats_) {
      task_runner->PostDelayedTask(
          FROM_HERE,
          base::BindOnce(&ParkableStringManager::
                             DropStringsWithCompressedDataAndRecordStatistics,
                         base::Unretained(this)),
          base::TimeDelta::FromSeconds(kParkingDelayInSeconds +
                                       kStatisticsRecordingDelayInSeconds));
      waiting_to_record_stats_ = true;
      should_record_stats_ = true;
    }
  } else {
    // See (2) above.
    if (waiting_to_record_stats_) {
      should_record_stats_ = false;
    }
  }
}

bool ParkableStringManager::IsRendererBackgrounded() const {
  DCHECK(IsMainThread());
  return backgrounded_;
}

bool ParkableStringManager::OnMemoryDump(
    base::trace_event::ProcessMemoryDump* pmd) {
  DCHECK(IsMainThread());
  base::trace_event::MemoryAllocatorDump* dump =
      pmd->CreateAllocatorDump("parkable_strings");

  size_t uncompressed_size = 0;
  size_t compressed_size = 0;
  size_t metadata_size = 0;
  size_t overhead_size = 0;

  for (ParkableStringImpl* str : unparked_strings_.Values()) {
    uncompressed_size += str->CharactersSizeInBytes();
    metadata_size += sizeof(ParkableStringImpl);

    if (str->has_compressed_data())
      overhead_size += str->compressed_size();
  }

  for (ParkableStringImpl* str : parked_strings_) {
    compressed_size += str->compressed_size();
    metadata_size += sizeof(ParkableStringImpl);
  }

  size_t total_size =
      uncompressed_size + compressed_size + metadata_size + overhead_size;
  dump->AddScalar("size", "bytes", total_size);
  dump->AddScalar("uncompressed_size", "bytes", uncompressed_size);
  dump->AddScalar("compressed_size", "bytes", compressed_size);
  dump->AddScalar("metadata_size", "bytes", metadata_size);
  dump->AddScalar("overhead_size", "bytes", overhead_size);

  pmd->AddSuballocation(dump->guid(),
                        WTF::Partitions::kAllocatedObjectPoolName);
  return true;
}

// static
bool ParkableStringManager::ShouldPark(const StringImpl& string) {
  // Don't attempt to park strings smaller than this size.
  static constexpr unsigned int kSizeThreshold = 10000;
  // TODO(lizeb): Consider parking non-main thread strings.
  return string.length() > kSizeThreshold && IsMainThread();
}

scoped_refptr<ParkableStringImpl> ParkableStringManager::Add(
    scoped_refptr<StringImpl>&& string) {
  DCHECK(IsMainThread());
  StringImpl* raw_ptr = string.get();
  auto it = unparked_strings_.find(raw_ptr);
  if (it != unparked_strings_.end())
    return it->value;

  auto new_parkable_string = base::MakeRefCounted<ParkableStringImpl>(
      std::move(string), ParkableStringImpl::ParkableState::kParkable);
  unparked_strings_.insert(raw_ptr, new_parkable_string.get());
  return new_parkable_string;
}

void ParkableStringManager::Remove(ParkableStringImpl* string,
                                   StringImpl* maybe_unparked_string) {
  DCHECK(IsMainThread());
  if (string->is_parked()) {
    auto it = parked_strings_.find(string);
    DCHECK(it != parked_strings_.end());
    parked_strings_.erase(it);
  } else {
    DCHECK(maybe_unparked_string);
    auto it = unparked_strings_.find(maybe_unparked_string);
    DCHECK(it != unparked_strings_.end());
    unparked_strings_.erase(it);
  }
}

void ParkableStringManager::OnParked(ParkableStringImpl* newly_parked_string,
                                     StringImpl* previous_unparked_string) {
  DCHECK(IsMainThread());
  DCHECK(newly_parked_string->is_parked());
  auto it = unparked_strings_.find(previous_unparked_string);
  DCHECK(it != unparked_strings_.end());
  unparked_strings_.erase(it);
  parked_strings_.insert(newly_parked_string);
}

void ParkableStringManager::OnUnparked(ParkableStringImpl* was_parked_string,
                                       StringImpl* new_unparked_string) {
  DCHECK(IsMainThread());
  DCHECK(!was_parked_string->is_parked());
  auto it = parked_strings_.find(was_parked_string);
  DCHECK(it != parked_strings_.end());
  parked_strings_.erase(it);
  unparked_strings_.insert(new_unparked_string, was_parked_string);
}

void ParkableStringManager::ParkAll(ParkableStringImpl::ParkingMode mode) {
  DCHECK(IsMainThread());
  DCHECK(base::FeatureList::IsEnabled(kCompressParkableStringsInBackground));

  size_t total_size = 0;
  for (ParkableStringImpl* str : parked_strings_)
    total_size += str->CharactersSizeInBytes();

  // Parking may be synchronous, need to copy values first.
  // In case of synchronous parking, |ParkableStringImpl::Park()| calls
  // |OnParked()|, which moves the string from |unparked_strings_|
  // to |parked_strings_|, hence the need to copy values first.
  //
  // Efficiency: In practice, either we are parking strings for the first time,
  // and |unparked_strings_| can contain a few 10s of strings (and we will
  // trigger expensive compression), or this is a subsequent one, and
  // |unparked_strings_| will have few entries.
  WTF::Vector<ParkableStringImpl*> unparked;
  unparked.ReserveCapacity(unparked_strings_.size());
  for (ParkableStringImpl* str : unparked_strings_.Values())
    unparked.push_back(str);

  for (ParkableStringImpl* str : unparked) {
    str->Park(mode);
    total_size += str->CharactersSizeInBytes();
  }

  // Only collect stats for "full" parking calls in background.
  if (mode == ParkableStringImpl::ParkingMode::kAlways &&
      IsRendererBackgrounded()) {
    size_t total_size_kb = total_size / 1000;
    UMA_HISTOGRAM_COUNTS_100000("Memory.MovableStringsTotalSizeKb",
                                total_size_kb);
    UMA_HISTOGRAM_COUNTS_1000("Memory.MovableStringsCount", Size());
  }
}

void ParkableStringManager::ParkAllIfRendererBackgrounded(
    ParkableStringImpl::ParkingMode mode) {
  DCHECK(IsMainThread());

  if (IsRendererBackgrounded())
    ParkAll(mode);
}

size_t ParkableStringManager::Size() const {
  return parked_strings_.size() + unparked_strings_.size();
}

void ParkableStringManager::DropStringsWithCompressedDataAndRecordStatistics() {
  DCHECK(IsMainThread());
  DCHECK(base::FeatureList::IsEnabled(kCompressParkableStringsInBackground));
  DCHECK(waiting_to_record_stats_);
  waiting_to_record_stats_ = false;
  if (!should_record_stats_)
    return;
  // See |SetRendererBackgrounded()|, is |should_record_stats_| is true then the
  // renderer is still backgrounded_.
  DCHECK(IsRendererBackgrounded());

  // We are in the background, drop all the ParkableStrings we can without
  // costing any CPU (as we already have the compressed representation).
  ParkAllIfRendererBackgrounded(
      ParkableStringImpl::ParkingMode::kIfCompressedDataExists);

  size_t total_size = 0, total_before_compression_size = 0;
  size_t total_compressed_size = 0;
  for (ParkableStringImpl* str : parked_strings_) {
    size_t size = str->CharactersSizeInBytes();
    total_size += size;
    total_before_compression_size += size;
    total_compressed_size += str->compressed_size();
  }

  for (ParkableStringImpl* str : unparked_strings_.Values())
    total_size += str->CharactersSizeInBytes();

  UMA_HISTOGRAM_COUNTS_100000("Memory.ParkableString.TotalSizeKb",
                              total_size / 1000);
  UMA_HISTOGRAM_COUNTS_100000("Memory.ParkableString.CompressedSizeKb",
                              total_compressed_size / 1000);
  size_t savings = total_before_compression_size - total_compressed_size;
  UMA_HISTOGRAM_COUNTS_100000("Memory.ParkableString.SavingsKb",
                              savings / 1000);
  if (total_before_compression_size != 0) {
    size_t ratio_percentage =
        (100 * total_compressed_size) / total_before_compression_size;
    UMA_HISTOGRAM_PERCENTAGE("Memory.ParkableString.CompressionRatio",
                             ratio_percentage);
  }
}

void ParkableStringManager::PurgeMemory() {
  DCHECK(IsMainThread());
  DCHECK(base::FeatureList::IsEnabled(kCompressParkableStringsInBackground));

  ParkAll(ParkableStringImpl::ParkingMode::kAlways);
  // Critical memory pressure: drop compressed data for strings that we cannot
  // park now.
  //
  // After |ParkAll()| has been called, parkable strings have either been parked
  // synchronously (and no longer in |unparked_strings_|), or being parked and
  // purging is a no-op.
  if (!IsRendererBackgrounded()) {
    for (ParkableStringImpl* str : unparked_strings_.Values())
      str->PurgeMemory();
  }
}

void ParkableStringManager::ResetForTesting() {
  backgrounded_ = false;
  waiting_to_record_stats_ = false;
  should_record_stats_ = false;
  unparked_strings_.clear();
  parked_strings_.clear();
}

ParkableStringManager::ParkableStringManager()
    : backgrounded_(false),
      waiting_to_record_stats_(false),
      should_record_stats_(false),
      unparked_strings_(),
      parked_strings_() {
  // No need to ever unregister, as the only ParkableStringManager instance
  // lives forever.
  MemoryCoordinator::Instance().RegisterClient(
      MakeGarbageCollected<OnPurgeMemoryListener>());
}

}  // namespace blink

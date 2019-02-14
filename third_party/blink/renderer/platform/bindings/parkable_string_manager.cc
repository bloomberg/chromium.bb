// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/bindings/parkable_string_manager.h"

#include <utility>

#include "base/bind.h"
#include "base/macros.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/single_thread_task_runner.h"
#include "base/trace_event/memory_allocator_dump.h"
#include "base/trace_event/process_memory_dump.h"
#include "base/trace_event/trace_event.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/platform/bindings/parkable_string.h"
#include "third_party/blink/renderer/platform/memory_pressure_listener.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread_scheduler.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"
#include "third_party/blink/renderer/platform/wtf/wtf.h"

namespace blink {

namespace {

enum class CompressionMode { kDisabled, kBackground, kForeground };

// Compression mode. Foreground parking takes precedence over background, and
// both are mutually exclusive.
CompressionMode GetCompressionMode() {
  if (base::FeatureList::IsEnabled(kCompressParkableStringsInForeground))
    return CompressionMode::kForeground;
  if (base::FeatureList::IsEnabled(kCompressParkableStringsInBackground))
    return CompressionMode::kBackground;
  return CompressionMode::kDisabled;
}

class OnPurgeMemoryListener : public GarbageCollected<OnPurgeMemoryListener>,
                              public MemoryPressureListener {
  USING_GARBAGE_COLLECTED_MIXIN(OnPurgeMemoryListener);

  void OnPurgeMemory() override {
    // Memory pressure compression is enabled for all modes.
    if (GetCompressionMode() == CompressionMode::kDisabled)
      return;
    ParkableStringManager::Instance().PurgeMemory();
  }
};

Vector<ParkableStringImpl*> GetUnparkedStrings(
    const HashMap<StringImpl*, ParkableStringImpl*, PtrHash<StringImpl>>&
        unparked_strings) {
  WTF::Vector<ParkableStringImpl*> unparked;
  unparked.ReserveCapacity(unparked_strings.size());
  for (ParkableStringImpl* str : unparked_strings.Values())
    unparked.push_back(str);

  return unparked;
}

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

  if (GetCompressionMode() != CompressionMode::kBackground)
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

  size_t original_size = 0;
  size_t uncompressed_size = 0;
  size_t compressed_size = 0;
  size_t metadata_size = 0;
  size_t overhead_size = 0;

  for (ParkableStringImpl* str : unparked_strings_.Values()) {
    original_size += str->CharactersSizeInBytes();
    uncompressed_size += str->CharactersSizeInBytes();
    metadata_size += sizeof(ParkableStringImpl);

    if (str->has_compressed_data())
      overhead_size += str->compressed_size();
  }

  for (ParkableStringImpl* str : parked_strings_) {
    original_size += str->CharactersSizeInBytes();
    compressed_size += str->compressed_size();
    metadata_size += sizeof(ParkableStringImpl);
  }

  size_t total_size =
      uncompressed_size + compressed_size + metadata_size + overhead_size;
  size_t memory_footprint =
      compressed_size + uncompressed_size + metadata_size + overhead_size;
  // Has to be uint64_t.
  size_t savings_size =
      original_size > memory_footprint ? original_size - memory_footprint : 0;
  dump->AddScalar("size", "bytes", total_size);
  dump->AddScalar("original_size", "bytes", original_size);
  dump->AddScalar("uncompressed_size", "bytes", uncompressed_size);
  dump->AddScalar("compressed_size", "bytes", compressed_size);
  dump->AddScalar("metadata_size", "bytes", metadata_size);
  dump->AddScalar("overhead_size", "bytes", overhead_size);
  dump->AddScalar("savings_size", "bytes", savings_size);

  pmd->AddSuballocation(dump->guid(),
                        WTF::Partitions::kAllocatedObjectPoolName);
  return true;
}

// static
bool ParkableStringManager::ShouldPark(const StringImpl& string) {
  // Don't attempt to park strings smaller than this size.
  static constexpr unsigned int kSizeThreshold = 10000;
  // TODO(lizeb): Consider parking non-main thread strings.
  return string.length() > kSizeThreshold && IsMainThread() &&
         GetCompressionMode() != CompressionMode::kDisabled;
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
  ScheduleAgingTaskIfNeeded();

  // Lazy registration because registering too early can cause crashes on Linux,
  // see crbug.com/930117, and registering without any strings is pointless
  // anyway.
  if (!did_register_memory_pressure_listener_) {
    // No need to ever unregister, as the only ParkableStringManager instance
    // lives forever.
    MemoryPressureListenerRegistry::Instance().RegisterClient(
        MakeGarbageCollected<OnPurgeMemoryListener>());
    did_register_memory_pressure_listener_ = true;
  }

  if (!has_posted_unparking_time_accounting_task_ &&
      GetCompressionMode() == CompressionMode::kForeground) {
    scoped_refptr<base::SingleThreadTaskRunner> task_runner =
        Thread::Current()->GetTaskRunner();
    DCHECK(task_runner);
    task_runner->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&ParkableStringManager::RecordUnparkingCpuCost,
                       base::Unretained(this)),
        base::TimeDelta::FromMinutes(5));
    has_posted_unparking_time_accounting_task_ = true;
  }

  return new_parkable_string;
}

void ParkableStringManager::Remove(ParkableStringImpl* string,
                                   StringImpl* maybe_unparked_string) {
  DCHECK(IsMainThread());
  DCHECK(string->may_be_parked());
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
  DCHECK(newly_parked_string->may_be_parked());
  DCHECK(newly_parked_string->is_parked());
  auto it = unparked_strings_.find(previous_unparked_string);
  DCHECK(it != unparked_strings_.end());
  unparked_strings_.erase(it);
  parked_strings_.insert(newly_parked_string);
}

void ParkableStringManager::OnUnparked(ParkableStringImpl* was_parked_string,
                                       StringImpl* new_unparked_string,
                                       base::TimeDelta unparking_time) {
  DCHECK(IsMainThread());
  DCHECK(was_parked_string->may_be_parked());
  DCHECK(!was_parked_string->is_parked());
  auto it = parked_strings_.find(was_parked_string);
  DCHECK(it != parked_strings_.end());
  parked_strings_.erase(it);
  unparked_strings_.insert(new_unparked_string, was_parked_string);
  total_unparking_time_ += unparking_time;
  ScheduleAgingTaskIfNeeded();
}

void ParkableStringManager::ParkAll(ParkableStringImpl::ParkingMode mode) {
  DCHECK(IsMainThread());
  DCHECK_NE(CompressionMode::kDisabled, GetCompressionMode());

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
  WTF::Vector<ParkableStringImpl*> unparked =
      GetUnparkedStrings(unparked_strings_);

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
  DCHECK_EQ(CompressionMode::kBackground, GetCompressionMode());
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

void ParkableStringManager::RecordUnparkingCpuCost() const {
  base::UmaHistogramTimes("Memory.ParkableString.MainThreadTime.5min",
                          total_unparking_time_);
}

void ParkableStringManager::AgeStringsAndPark() {
  if (GetCompressionMode() != CompressionMode::kForeground)
    return;

  TRACE_EVENT0("blink", "ParkableStringManager::AgeStringsAndPark");
  has_pending_aging_task_ = false;

  WTF::Vector<ParkableStringImpl*> unparked =
      GetUnparkedStrings(unparked_strings_);

  bool can_make_progress = false;
  for (ParkableStringImpl* str : unparked) {
    if (str->MaybeAgeOrParkString() ==
        ParkableStringImpl::AgeOrParkResult::kSuccessOrTransientFailure)
      can_make_progress = true;
  }

  // Some strings will never be parkable because there are lasting external
  // references to them. Don't endlessely reschedule the aging task if we are
  // not making progress (that is, no new string was either aged or parked).
  //
  // This ensures that the tasks will stop getting scheduled, assuming that
  // the renderer is otherwise idle. Note that we cannot use "idle" tasks as
  // we need to age and park strings after the renderer becomes idle, meaning
  // that this has to run when the idle tasks are not. As a consequence, it
  // is important to make sure that this will not reschedule tasks forever.
  bool reschedule = !unparked_strings_.IsEmpty() && can_make_progress;
  if (reschedule)
    ScheduleAgingTaskIfNeeded();
}

void ParkableStringManager::ScheduleAgingTaskIfNeeded() {
  if (GetCompressionMode() != CompressionMode::kForeground)
    return;

  if (has_pending_aging_task_)
    return;

  scoped_refptr<base::SingleThreadTaskRunner> task_runner =
      Thread::Current()->GetTaskRunner();
  task_runner->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&ParkableStringManager::AgeStringsAndPark,
                     base::Unretained(this)),
      base::TimeDelta::FromSeconds(kAgingIntervalInSeconds));
  has_pending_aging_task_ = true;
}

void ParkableStringManager::PurgeMemory() {
  DCHECK(IsMainThread());
  DCHECK_NE(CompressionMode::kDisabled, GetCompressionMode());

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
  has_pending_aging_task_ = false;
  should_record_stats_ = false;
  has_posted_unparking_time_accounting_task_ = false;
  did_register_memory_pressure_listener_ = false;
  total_unparking_time_ = base::TimeDelta();
  unparked_strings_.clear();
  parked_strings_.clear();
}

ParkableStringManager::ParkableStringManager()
    : backgrounded_(false),
      waiting_to_record_stats_(false),
      has_pending_aging_task_(false),
      should_record_stats_(false),
      has_posted_unparking_time_accounting_task_(false),
      did_register_memory_pressure_listener_(false),
      total_unparking_time_(),
      unparked_strings_(),
      parked_strings_() {}

}  // namespace blink

// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_BINDINGS_PARKABLE_STRING_MANAGER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_BINDINGS_PARKABLE_STRING_MANAGER_H_

#include <memory>
#include <utility>

#include "base/feature_list.h"
#include "base/gtest_prod_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/time/time.h"
#include "base/trace_event/memory_dump_provider.h"
#include "third_party/blink/renderer/platform/bindings/parkable_string.h"
#include "third_party/blink/renderer/platform/disk_data_allocator.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/hash_functions.h"
#include "third_party/blink/renderer/platform/wtf/hash_map.h"
#include "third_party/blink/renderer/platform/wtf/hash_set.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class DiskDataAllocator;
class ParkableString;

class PLATFORM_EXPORT ParkableStringManagerDumpProvider
    : public base::trace_event::MemoryDumpProvider {
  USING_FAST_MALLOC(ParkableStringManagerDumpProvider);

 public:
  static ParkableStringManagerDumpProvider* Instance();
  ParkableStringManagerDumpProvider(const ParkableStringManagerDumpProvider&) =
      delete;
  ParkableStringManagerDumpProvider& operator=(
      const ParkableStringManagerDumpProvider&) = delete;
  ~ParkableStringManagerDumpProvider() override;

  bool OnMemoryDump(const base::trace_event::MemoryDumpArgs&,
                    base::trace_event::ProcessMemoryDump*) override;

 private:
  ParkableStringManagerDumpProvider();
};

// Manages all the ParkableStrings, and parks eligible strings after the
// renderer has been backgrounded.
// Main Thread only.
class PLATFORM_EXPORT ParkableStringManager {
  USING_FAST_MALLOC(ParkableStringManager);

 public:
  struct Statistics;

  static ParkableStringManager& Instance();
  ParkableStringManager(const ParkableStringManager&) = delete;
  ParkableStringManager& operator=(const ParkableStringManager&) = delete;
  ~ParkableStringManager();

  void PurgeMemory();
  // Number of parked and unparked strings. Public for testing.
  size_t Size() const;

  bool OnMemoryDump(base::trace_event::ProcessMemoryDump* pmd);

  // Whether a string is parkable or not. Can be called from any thread.
  static bool ShouldPark(const StringImpl& string);

  // Public for testing.
  constexpr static int kAgingIntervalInSeconds = 2;

  // According to UMA data (as of 2021-11-09) ~70% of renderers exist for less
  // than 60 seconds. Using this as a delay of the first parking attempts
  // to lower the number of parking operations on strings that are discarded
  // soon after when the renderer dies. Subsequent additions will schedule
  // according to the default delay. This applies to all renderers, young or
  // old. This is considered a necessary trade-off to cover the spare renderer
  // use-case. The spare renderer might have a long lifetime on first addition
  // of a parkable string but it does not indicate a higher chance of the
  // renderer staying alive for a long time.
  constexpr static base::TimeDelta kFirstParkingDelay{base::Seconds(60)};

  static const char* kAllocatorDumpName;
  // Relies on secure hash equality for deduplication. If one day SHA256 becomes
  // insecure, then this would need to be updated to a more robust hash.
  struct SecureDigestHash;
  using StringMap = WTF::HashMap<const ParkableStringImpl::SecureDigest*,
                                 ParkableStringImpl*,
                                 SecureDigestHash>;

 private:
  friend class ParkableString;
  friend class ParkableStringImpl;

  scoped_refptr<ParkableStringImpl> Add(scoped_refptr<StringImpl>&&);
  void Remove(ParkableStringImpl*);

  void OnParked(ParkableStringImpl*);
  void OnWrittenToDisk(ParkableStringImpl*);
  void OnReadFromDisk(ParkableStringImpl*);
  void OnUnparked(ParkableStringImpl*);

  void ParkAll(ParkableStringImpl::ParkingMode mode);
  void RecordStatisticsAfter5Minutes() const;
  void AgeStringsAndPark();
  void ScheduleAgingTaskIfNeeded();

  void RecordUnparkingTime(base::TimeDelta unparking_time) {
    total_unparking_time_ += unparking_time;
  }
  void RecordParkingThreadTime(base::TimeDelta parking_thread_time) {
    total_parking_thread_time_ += parking_thread_time;
  }
  void RecordDiskWriteTime(base::TimeDelta write_time) {
    total_disk_write_time_ += write_time;
  }
  void RecordDiskReadTime(base::TimeDelta read_time) {
    total_disk_read_time_ += read_time;
  }

  Statistics ComputeStatistics() const;

  DiskDataAllocator& data_allocator() const {
    if (allocator_for_testing_)
      return *allocator_for_testing_;

    return DiskDataAllocator::Instance();
  }

  void SetDataAllocatorForTesting(
      std::unique_ptr<DiskDataAllocator> allocator) {
    allocator_for_testing_ = std::move(allocator);
  }

  void ResetForTesting();
  ParkableStringManager();

  bool has_pending_aging_task_;
  bool has_posted_unparking_time_accounting_task_;
  bool did_register_memory_pressure_listener_;
  base::TimeDelta total_unparking_time_;
  base::TimeDelta total_parking_thread_time_;
  base::TimeDelta total_disk_read_time_;
  base::TimeDelta total_disk_write_time_;

  StringMap unparked_strings_;
  StringMap parked_strings_;
  StringMap on_disk_strings_;

  bool first_string_aging_was_delayed_ = false;

  std::unique_ptr<DiskDataAllocator> allocator_for_testing_;

  friend class ParkableStringTest;
  FRIEND_TEST_ALL_PREFIXES(ParkableStringTest, SynchronousCompression);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_BINDINGS_PARKABLE_STRING_MANAGER_H_

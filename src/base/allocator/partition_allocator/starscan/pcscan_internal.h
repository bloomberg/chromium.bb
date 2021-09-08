// Copyright (c) 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_ALLOCATOR_PARTITION_ALLOCATOR_STARSCAN_PCSCAN_INTERNAL_H_
#define BASE_ALLOCATOR_PARTITION_ALLOCATOR_STARSCAN_PCSCAN_INTERNAL_H_

#include <array>
#include <mutex>
#include <unordered_map>

#include "base/allocator/partition_allocator/starscan/metadata_allocator.h"
#include "base/allocator/partition_allocator/starscan/pcscan.h"
#include "base/allocator/partition_allocator/starscan/starscan_fwd.h"
#include "base/allocator/partition_allocator/starscan/write_protector.h"
#include "base/memory/scoped_refptr.h"
#include "base/no_destructor.h"

namespace base {

namespace internal {

class PCScanTask;

// Internal PCScan singleton. The separation between frontend and backend is
// needed to keep access to the hot data (quarantine) in the frontend fast,
// whereas the backend can hold cold data.
class PCScanInternal final {
 public:
  using Root = PCScan::Root;
  using TaskHandle = scoped_refptr<PCScanTask>;

  static constexpr size_t kMaxNumberOfRoots = 8u;
  class Roots final : private std::array<Root*, kMaxNumberOfRoots> {
    using Base = std::array<Root*, kMaxNumberOfRoots>;

   public:
    using typename Base::const_iterator;
    using typename Base::iterator;

    // Explicitly value-initialize Base{} as otherwise the default
    // (aggregate) initialization won't be considered as constexpr.
    constexpr Roots() : Base{} {}

    iterator begin() { return Base::begin(); }
    const_iterator begin() const { return Base::begin(); }

    iterator end() { return begin() + current_; }
    const_iterator end() const { return begin() + current_; }

    void Add(Root* root);

    size_t size() const { return current_; }

    void ClearForTesting();  // IN-TEST

   private:
    size_t current_ = 0u;
  };

  static PCScanInternal& Instance() {
    // Since the data that PCScanInternal holds is cold, it's fine to have the
    // runtime check for thread-safe local static initialization.
    static base::NoDestructor<PCScanInternal> instance;
    return *instance;
  }

  PCScanInternal(const PCScanInternal&) = delete;
  PCScanInternal& operator=(const PCScanInternal&) = delete;

  ~PCScanInternal();

  void Initialize(PCScan::WantedWriteProtectionMode);
  bool is_initialized() const { return is_initialized_; }

  void PerformScan(PCScan::InvocationMode);
  void PerformScanIfNeeded(PCScan::InvocationMode);
  void PerformDelayedScan(TimeDelta delay);
  void JoinScan();

  TaskHandle CurrentPCScanTask() const;
  void SetCurrentPCScanTask(TaskHandle task);
  void ResetCurrentPCScanTask();

  void RegisterScannableRoot(Root* root);
  void RegisterNonScannableRoot(Root* root);

  Roots& scannable_roots() { return scannable_roots_; }
  const Roots& scannable_roots() const { return scannable_roots_; }

  Roots& nonscannable_roots() { return nonscannable_roots_; }
  const Roots& nonscannable_roots() const { return nonscannable_roots_; }

  void SetProcessName(const char* name);
  const char* process_name() const { return process_name_; }

  // Get size of all committed pages from scannable and nonscannable roots.
  size_t CalculateTotalHeapSize() const;

  SimdSupport simd_support() const { return simd_support_; }

  void EnableStackScanning();
  void DisableStackScanning();
  bool IsStackScanningEnabled() const;

  void NotifyThreadCreated(void* stack_top);
  void NotifyThreadDestroyed();

  void* GetCurrentThreadStackTop() const;

  void ProtectPages(uintptr_t begin, size_t size);
  void UnprotectPages(uintptr_t begin, size_t size);

  void ClearRootsForTesting();                               // IN-TEST
  void ReinitForTesting(PCScan::WantedWriteProtectionMode);  // IN-TEST
  void FinishScanForTesting();                               // IN-TEST

 private:
  friend base::NoDestructor<PCScanInternal>;

  using StackTops = std::unordered_map<
      PlatformThreadId,
      void*,
      std::hash<PlatformThreadId>,
      std::equal_to<>,
      MetadataAllocator<std::pair<const PlatformThreadId, void*>>>;

  PCScanInternal();

  TaskHandle current_task_;
  mutable std::mutex current_task_mutex_;

  Roots scannable_roots_{};
  Roots nonscannable_roots_{};

  bool stack_scanning_enabled_{false};
  // TLS emulation of stack tops. Since this is guaranteed to go through
  // non-quarantinable partition, using it from safepoints is safe.
  StackTops stack_tops_;
  mutable std::mutex stack_tops_mutex_;

  const char* process_name_ = nullptr;
  const SimdSupport simd_support_;

  std::unique_ptr<WriteProtector> write_protector_;

  bool is_initialized_ = false;
};

}  // namespace internal

}  // namespace base

#endif  // BASE_ALLOCATOR_PARTITION_ALLOCATOR_STARSCAN_PCSCAN_INTERNAL_H_

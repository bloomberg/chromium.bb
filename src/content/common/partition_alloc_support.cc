// Copyright (c) 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/partition_alloc_support.h"

#include <string>

#include "base/allocator/allocator_shim.h"
#include "base/allocator/allocator_shim_default_dispatch_to_partition_alloc.h"
#include "base/allocator/buildflags.h"
#include "base/allocator/partition_alloc_features.h"
#include "base/allocator/partition_alloc_support.h"
#include "base/allocator/partition_allocator/partition_alloc_config.h"
#include "base/allocator/partition_allocator/starscan/pcscan.h"
#include "base/allocator/partition_allocator/starscan/pcscan_scheduling.h"
#include "base/allocator/partition_allocator/starscan/stack/stack.h"
#include "base/allocator/partition_allocator/thread_cache.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/feature_list.h"
#include "base/location.h"
#include "base/memory/nonscannable_memory.h"
#include "base/no_destructor.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "content/public/common/content_switches.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/system/sys_info.h"
#endif

#if BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
#include "base/allocator/partition_allocator/memory_reclaimer.h"
#include "base/threading/thread_task_runner_handle.h"
#endif

namespace content {
namespace internal {

namespace {

void SetProcessNameForPCScan(const std::string& process_type) {
  const char* name = [&process_type] {
    if (process_type.empty()) {
      // Empty means browser process.
      return "Browser";
    }
    if (process_type == switches::kRendererProcess)
      return "Renderer";
    if (process_type == switches::kGpuProcess)
      return "Gpu";
    if (process_type == switches::kUtilityProcess)
      return "Utility";
    return static_cast<const char*>(nullptr);
  }();

  if (name) {
    base::internal::PCScan::SetProcessName(name);
  }
}

bool EnablePCScanForMallocPartitionsIfNeeded() {
#if BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC) && defined(PA_ALLOW_PCSCAN)
  using Config = base::internal::PCScan::InitConfig;
  DCHECK(base::FeatureList::GetInstance());
  if (base::FeatureList::IsEnabled(base::features::kPartitionAllocPCScan)) {
    base::allocator::EnablePCScan({Config::WantedWriteProtectionMode::kEnabled,
                                   Config::SafepointMode::kEnabled});
    base::allocator::RegisterPCScanStatsReporter();
    return true;
  }
#endif  // BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC) && defined(PA_ALLOW_PCSCAN)
  return false;
}

bool EnablePCScanForMallocPartitionsInBrowserProcessIfNeeded() {
#if BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC) && defined(PA_ALLOW_PCSCAN)
  using Config = base::internal::PCScan::InitConfig;
  DCHECK(base::FeatureList::GetInstance());
  if (base::FeatureList::IsEnabled(
          base::features::kPartitionAllocPCScanBrowserOnly)) {
    const Config::WantedWriteProtectionMode wp_mode =
        base::FeatureList::IsEnabled(base::features::kPartitionAllocDCScan)
            ? Config::WantedWriteProtectionMode::kEnabled
            : Config::WantedWriteProtectionMode::kDisabled;
#if !defined(PA_STARSCAN_UFFD_WRITE_PROTECTOR_SUPPORTED)
    CHECK_EQ(Config::WantedWriteProtectionMode::kDisabled, wp_mode)
        << "DCScan is currently only supported on Linux based systems";
#endif
    base::allocator::EnablePCScan({wp_mode, Config::SafepointMode::kEnabled});
    base::allocator::RegisterPCScanStatsReporter();
    return true;
  }
#endif  // BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC) && defined(PA_ALLOW_PCSCAN)
  return false;
}

bool EnablePCScanForMallocPartitionsInRendererProcessIfNeeded() {
#if BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC) && defined(PA_ALLOW_PCSCAN)
  using Config = base::internal::PCScan::InitConfig;
  DCHECK(base::FeatureList::GetInstance());
  if (base::FeatureList::IsEnabled(
          base::features::kPartitionAllocPCScanRendererOnly)) {
    const Config::WantedWriteProtectionMode wp_mode =
        base::FeatureList::IsEnabled(base::features::kPartitionAllocDCScan)
            ? Config::WantedWriteProtectionMode::kEnabled
            : Config::WantedWriteProtectionMode::kDisabled;
#if !defined(PA_STARSCAN_UFFD_WRITE_PROTECTOR_SUPPORTED)
    CHECK_EQ(Config::WantedWriteProtectionMode::kDisabled, wp_mode)
        << "DCScan is currently only supported on Linux based systems";
#endif
    base::allocator::EnablePCScan({wp_mode, Config::SafepointMode::kDisabled});
    base::allocator::RegisterPCScanStatsReporter();
    return true;
  }
#endif  // BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC) && defined(PA_ALLOW_PCSCAN)
  return false;
}

}  // namespace

void ReconfigurePartitionForKnownProcess(const std::string& process_type) {
  DCHECK_NE(process_type, switches::kZygoteProcess);
  // TODO(keishi): Move the code to enable BRP back here after Finch
  // experiments.
}

PartitionAllocSupport::PartitionAllocSupport() = default;

void PartitionAllocSupport::ReconfigureEarlyish(
    const std::string& process_type) {
  {
    base::AutoLock scoped_lock(lock_);
    // TODO(bartekn): Switch to DCHECK once confirmed there are no issues.
    CHECK(!called_earlyish_)
        << "ReconfigureEarlyish was already called for process '"
        << established_process_type_ << "'; current process: '" << process_type
        << "'";

    called_earlyish_ = true;
    established_process_type_ = process_type;
  }

  if (process_type != switches::kZygoteProcess) {
    ReconfigurePartitionForKnownProcess(process_type);
  }

  // These initializations are only relevant for PartitionAlloc-Everywhere
  // builds.
#if BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
  base::allocator::EnablePartitionAllocMemoryReclaimer();
#endif  // BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
}

void PartitionAllocSupport::ReconfigureAfterZygoteFork(
    const std::string& process_type) {
  {
    base::AutoLock scoped_lock(lock_);
    // TODO(bartekn): Switch to DCHECK once confirmed there are no issues.
    CHECK(!called_after_zygote_fork_)
        << "ReconfigureAfterZygoteFork was already called for process '"
        << established_process_type_ << "'; current process: '" << process_type
        << "'";
    DCHECK(called_earlyish_)
        << "Attempt to call ReconfigureAfterZygoteFork without calling "
           "ReconfigureEarlyish; current process: '"
        << process_type << "'";
    DCHECK_EQ(established_process_type_, switches::kZygoteProcess)
        << "Attempt to call ReconfigureAfterZygoteFork while "
           "ReconfigureEarlyish was called on non-zygote process '"
        << established_process_type_ << "'; current process: '" << process_type
        << "'";

    called_after_zygote_fork_ = true;
    established_process_type_ = process_type;
  }

  if (process_type != switches::kZygoteProcess) {
    ReconfigurePartitionForKnownProcess(process_type);
  }
}

void PartitionAllocSupport::ReconfigureAfterFeatureListInit(
    const std::string& process_type) {
  {
    base::AutoLock scoped_lock(lock_);
    // Avoid initializing more than once.
    // TODO(bartekn): See if can be converted to (D)CHECK.
    if (called_after_feature_list_init_) {
      DCHECK_EQ(established_process_type_, process_type)
          << "ReconfigureAfterFeatureListInit was already called for process '"
          << established_process_type_ << "'; current process: '"
          << process_type << "'";
      return;
    }
    DCHECK(called_earlyish_)
        << "Attempt to call ReconfigureAfterFeatureListInit without calling "
           "ReconfigureEarlyish; current process: '"
        << process_type << "'";
    DCHECK_NE(established_process_type_, switches::kZygoteProcess)
        << "Attempt to call ReconfigureAfterFeatureListInit without calling "
           "ReconfigureAfterZygoteFork; current process: '"
        << process_type << "'";
    DCHECK_EQ(established_process_type_, process_type)
        << "ReconfigureAfterFeatureListInit wasn't called for an already "
           "established process '"
        << established_process_type_ << "'; current process: '" << process_type
        << "'";

    called_after_feature_list_init_ = true;
  }

  DCHECK_NE(process_type, switches::kZygoteProcess);
  // TODO(bartekn): Switch to DCHECK once confirmed there are no issues.
  CHECK(base::FeatureList::GetInstance());

  bool enable_brp = false;
  [[maybe_unused]] bool split_main_partition = false;
  [[maybe_unused]] bool use_dedicated_aligned_partition = false;
#if BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
#if BUILDFLAG(USE_BACKUP_REF_PTR)
  bool process_affected_by_brp_flag = false;
  if (base::FeatureList::IsEnabled(
          base::features::kPartitionAllocBackupRefPtr)) {
    // No specified process type means this is the Browser process.
    switch (base::features::kBackupRefPtrEnabledProcessesParam.Get()) {
      case base::features::BackupRefPtrEnabledProcesses::kBrowserOnly:
        process_affected_by_brp_flag = process_type.empty();
        break;
      case base::features::BackupRefPtrEnabledProcesses::kBrowserAndRenderer:
        process_affected_by_brp_flag =
            process_type.empty() ||
            (process_type == switches::kRendererProcess);
        break;
      case base::features::BackupRefPtrEnabledProcesses::kNonRenderer:
        process_affected_by_brp_flag =
            (process_type != switches::kRendererProcess);
        break;
      case base::features::BackupRefPtrEnabledProcesses::kAllProcesses:
        process_affected_by_brp_flag = true;
        break;
    }
  }

  if (process_affected_by_brp_flag) {
    switch (base::features::kBackupRefPtrModeParam.Get()) {
      case base::features::BackupRefPtrMode::kDisabled:
        // Do nothing. Equivalent to !IsEnabled(kPartitionAllocBackupRefPtr).
        break;

      case base::features::BackupRefPtrMode::kEnabled:
        enable_brp = true;
        split_main_partition = true;
#if !BUILDFLAG(PUT_REF_COUNT_IN_PREVIOUS_SLOT)
        // AlignedAlloc relies on natural alignment offered by the allocator
        // (see the comment inside PartitionRoot::AlignedAllocFlags). Any extras
        // in front of the allocation will mess up that alignment. Such extras
        // are used when BackupRefPtr is on, in which case, we need a separate
        // partition, dedicated to handle only aligned allocations, where those
        // extras are disabled. However, if the "previous slot" variant is used,
        // no dedicated partition is needed, as the extras won't interfere with
        // the alignment requirements.
        use_dedicated_aligned_partition = true;
#endif
        break;

      case base::features::BackupRefPtrMode::kDisabledButSplitPartitions2Way:
        split_main_partition = true;
        break;

      case base::features::BackupRefPtrMode::kDisabledButSplitPartitions3Way:
        split_main_partition = true;
        use_dedicated_aligned_partition = true;
        break;
    }
  }
#endif  // BUILDFLAG(USE_BACKUP_REF_PTR)
#endif  // BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)

#if BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
  base::allocator::ConfigurePartitions(
      base::allocator::EnableBrp(enable_brp),
      base::allocator::SplitMainPartition(split_main_partition),
      base::allocator::UseDedicatedAlignedPartition(
          use_dedicated_aligned_partition));
#endif  // BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)

  // If BRP is not enabled, check if any of PCScan flags is enabled.
  bool scan_enabled = false;
  if (!enable_brp) {
    scan_enabled = EnablePCScanForMallocPartitionsIfNeeded();
    // No specified process type means this is the Browser process.
    if (process_type.empty()) {
      scan_enabled = scan_enabled ||
                     EnablePCScanForMallocPartitionsInBrowserProcessIfNeeded();
    }
    if (process_type == switches::kRendererProcess) {
      scan_enabled = scan_enabled ||
                     EnablePCScanForMallocPartitionsInRendererProcessIfNeeded();
    }
    if (scan_enabled) {
      if (base::FeatureList::IsEnabled(
              base::features::kPartitionAllocPCScanStackScanning)) {
#if BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
        base::internal::PCScan::EnableStackScanning();
        // Notify PCScan about the main thread.
        base::internal::PCScan::NotifyThreadCreated(
            base::internal::GetStackTop());
#endif
      }
      if (base::FeatureList::IsEnabled(
              base::features::kPartitionAllocPCScanImmediateFreeing)) {
        base::internal::PCScan::EnableImmediateFreeing();
      }
      if (base::FeatureList::IsEnabled(
              base::features::kPartitionAllocPCScanEagerClearing)) {
        base::internal::PCScan::SetClearType(
            base::internal::PCScan::ClearType::kEager);
      }
      SetProcessNameForPCScan(process_type);
    }
  }

#if BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
  // Non-quarantinable partition is dealing with hot V8's zone allocations.
  // In case PCScan is enabled in Renderer, enable thread cache on this
  // partition. At the same time, thread cache on the main(malloc) partition
  // must be disabled, because only one partition can have it on.
  if (scan_enabled && process_type == switches::kRendererProcess) {
    base::internal::NonQuarantinableAllocator::Instance()
        .root()
        ->EnableThreadCacheIfSupported();
  } else {
    base::internal::PartitionAllocMalloc::Allocator()
        ->EnableThreadCacheIfSupported();
  }

  if (base::FeatureList::IsEnabled(
          base::features::kPartitionAllocLargeEmptySlotSpanRing)) {
    base::internal::PartitionAllocMalloc::Allocator()
        ->EnableLargeEmptySlotSpanRing();
    base::internal::PartitionAllocMalloc::AlignedAllocator()
        ->EnableLargeEmptySlotSpanRing();
  }
#endif  // BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
}

void PartitionAllocSupport::ReconfigureAfterTaskRunnerInit(
    const std::string& process_type) {
  {
    base::AutoLock scoped_lock(lock_);

    // Init only once.
    if (called_after_thread_pool_init_)
      return;

    DCHECK_EQ(established_process_type_, process_type);
    // Enforce ordering.
    DCHECK(called_earlyish_);
    DCHECK(called_after_feature_list_init_);

    called_after_thread_pool_init_ = true;
  }

#if defined(PA_THREAD_CACHE_SUPPORTED) && \
    BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
  // This should be called in specific processes, as the main thread is
  // initialized later.
  DCHECK(process_type != switches::kZygoteProcess);

  base::allocator::StartThreadCachePeriodicPurge();

#if BUILDFLAG(IS_ANDROID)
  // Lower thread cache limits to avoid stranding too much memory in the caches.
  if (base::SysInfo::IsLowEndDevice()) {
    base::internal::ThreadCacheRegistry::Instance().SetThreadCacheMultiplier(
        base::internal::ThreadCache::kDefaultMultiplier / 2.);
  }
#endif  // BUILDFLAG(IS_ANDROID)

  // Renderer processes are more performance-sensitive, increase thread cache
  // limits.
  if (process_type == switches::kRendererProcess &&
      base::FeatureList::IsEnabled(
          base::features::kPartitionAllocLargeThreadCacheSize)) {
    largest_cached_size_ =
        base::internal::ThreadCacheLimits::kLargeSizeThreshold;

#if BUILDFLAG(IS_ANDROID) && defined(ARCH_CPU_32_BITS)
    // Devices almost always report less physical memory than what they actually
    // have, so anything above 3GiB will catch 4GiB and above.
    if (base::SysInfo::AmountOfPhysicalMemory() <= int64_t{3500} * 1024 * 1024)
      largest_cached_size_ =
          base::internal::ThreadCacheLimits::kDefaultSizeThreshold;
#endif  // BUILDFLAG(IS_ANDROID) && !defined(ARCH_CPU_64_BITS)

    base::internal::ThreadCache::SetLargestCachedSize(largest_cached_size_);
  }

#endif  // defined(PA_THREAD_CACHE_SUPPORTED) &&
        // BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)

  if (base::FeatureList::IsEnabled(
          base::features::kPartitionAllocPCScanMUAwareScheduler)) {
    // Assign PCScan a task-based scheduling backend.
    static base::NoDestructor<base::internal::MUAwareTaskBasedBackend>
        mu_aware_task_based_backend{
            base::internal::PCScan::scheduler(),
            &base::internal::PCScan::PerformDelayedScan};
    base::internal::PCScan::scheduler().SetNewSchedulingBackend(
        *mu_aware_task_based_backend.get());
  }

#if BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
  base::allocator::StartMemoryReclaimer(base::ThreadTaskRunnerHandle::Get());
#endif
}

void PartitionAllocSupport::OnForegrounded() {
#if defined(PA_THREAD_CACHE_SUPPORTED) && \
    BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
  {
    base::AutoLock scoped_lock(lock_);
    if (established_process_type_ != switches::kRendererProcess)
      return;
  }

  base::internal::ThreadCache::SetLargestCachedSize(largest_cached_size_);
#endif  // defined(PA_THREAD_CACHE_SUPPORTED) &&
        // BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
}

void PartitionAllocSupport::OnBackgrounded() {
#if defined(PA_THREAD_CACHE_SUPPORTED) && \
    BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
  {
    base::AutoLock scoped_lock(lock_);
    if (established_process_type_ != switches::kRendererProcess)
      return;
  }

  // Performance matters less for background renderers, don't pay the memory
  // cost.
  base::internal::ThreadCache::SetLargestCachedSize(
      base::internal::ThreadCacheLimits::kDefaultSizeThreshold);

  // In renderers, memory reclaim uses the "idle time" task runner to run
  // periodic reclaim. This does not always run when the renderer is idle, and
  // in particular after the renderer gets backgrounded. As a result, empty slot
  // spans are potentially never decommitted. To mitigate that, run a one-off
  // reclaim a few seconds later. Even if the renderer comes back to foreground
  // in the meantime, the worst case is a few more system calls.
  //
  // TODO(lizeb): Remove once/if the behavior of idle tasks changes.
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, base::BindOnce([]() {
        base::PartitionAllocMemoryReclaimer::Instance()->ReclaimAll();
      }),
      base::Seconds(10));

#endif  // defined(PA_THREAD_CACHE_SUPPORTED) &&
        // BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
}

}  // namespace internal
}  // namespace content

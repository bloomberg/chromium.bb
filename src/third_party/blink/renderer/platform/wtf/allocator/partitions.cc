/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/platform/wtf/allocator/partitions.h"

#include "base/allocator/buildflags.h"
#include "base/allocator/partition_allocator/memory_reclaimer.h"
#include "base/allocator/partition_allocator/oom.h"
#include "base/allocator/partition_allocator/page_allocator.h"
#include "base/allocator/partition_allocator/partition_alloc.h"
#include "base/allocator/partition_allocator/partition_alloc_config.h"
#include "base/allocator/partition_allocator/partition_alloc_features.h"
#include "base/allocator/partition_allocator/partition_root.h"
#include "base/debug/alias.h"
#include "base/feature_list.h"
#include "base/no_destructor.h"
#include "base/strings/safe_sprintf.h"
#include "base/thread_annotations.h"
#include "components/crash/core/common/crash_key.h"
#include "third_party/blink/renderer/platform/wtf/allocator/partition_allocator.h"
#include "third_party/blink/renderer/platform/wtf/wtf.h"

namespace WTF {

const char* const Partitions::kAllocatedObjectPoolName =
    "partition_alloc/allocated_objects";

#if defined(PA_ALLOW_PCSCAN)
// Runs PCScan on WTF partitions.
const base::Feature kPCScanBlinkPartitions{"PCScanBlinkPartitions",
                                           base::FEATURE_DISABLED_BY_DEFAULT};
#endif

bool Partitions::initialized_ = false;

// These statics are inlined, so cannot be LazyInstances. We create the values,
// and then set the pointers correctly in Initialize().
#if !BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
base::ThreadSafePartitionRoot* Partitions::fast_malloc_root_ = nullptr;
#endif
base::ThreadSafePartitionRoot* Partitions::array_buffer_root_ = nullptr;
base::ThreadSafePartitionRoot* Partitions::buffer_root_ = nullptr;
base::ThreadUnsafePartitionRoot* Partitions::layout_root_ = nullptr;

// static
void Partitions::Initialize() {
  static bool initialized = InitializeOnce();
  DCHECK(initialized);
}

// static
bool Partitions::InitializeOnce() {
#if !BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
  static base::NoDestructor<base::PartitionAllocator> fast_malloc_allocator{};
  fast_malloc_allocator->init({
    base::PartitionOptions::AlignedAlloc::kDisallowed,
        base::PartitionOptions::ThreadCache::kEnabled,
        base::PartitionOptions::Quarantine::kAllowed,
        base::PartitionOptions::Cookies::kAllowed,
#if BUILDFLAG(ENABLE_BACKUP_REF_PTR_IN_RENDERER_PROCESS)
        base::PartitionOptions::RefCount::kAllowed
#else
        base::PartitionOptions::RefCount::kDisallowed
#endif
  });

  fast_malloc_root_ = fast_malloc_allocator->root();
#endif  // !BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)

  static base::NoDestructor<base::PartitionAllocator> array_buffer_allocator{};
  static base::NoDestructor<base::PartitionAllocator> buffer_allocator{};
  static base::NoDestructor<base::ThreadUnsafePartitionAllocator>
      layout_allocator{};

  base::PartitionAllocGlobalInit(&Partitions::HandleOutOfMemory);

  // RefCount disallowed because it will prevent allocations from being 16B
  // aligned as required by ArrayBufferContents.
  array_buffer_allocator->init({
    base::PartitionOptions::AlignedAlloc::kDisallowed,
        base::PartitionOptions::ThreadCache::kDisabled,
        base::PartitionOptions::Quarantine::kAllowed,
        base::PartitionOptions::Cookies::kAllowed,
        base::PartitionOptions::RefCount::kDisallowed
  });
  buffer_allocator->init({
    base::PartitionOptions::AlignedAlloc::kDisallowed,
        base::PartitionOptions::ThreadCache::kDisabled,
        base::PartitionOptions::Quarantine::kAllowed,
        base::PartitionOptions::Cookies::kAllowed,
#if BUILDFLAG(ENABLE_BACKUP_REF_PTR_IN_RENDERER_PROCESS)
        base::PartitionOptions::RefCount::kAllowed
#else
        base::PartitionOptions::RefCount::kDisallowed
#endif
  });
  // RefCount disallowed because layout code will be excluded from raw_ptr<T>
  // rewrite due to performance.
  layout_allocator->init({
    base::PartitionOptions::AlignedAlloc::kDisallowed,
        base::PartitionOptions::ThreadCache::kDisabled,
        base::PartitionOptions::Quarantine::kAllowed,
        base::PartitionOptions::Cookies::kAllowed,
        base::PartitionOptions::RefCount::kDisallowed
  });

  array_buffer_root_ = array_buffer_allocator->root();
  buffer_root_ = buffer_allocator->root();
  layout_root_ = layout_allocator->root();

#if defined(PA_ALLOW_PCSCAN)
  if (base::FeatureList::IsEnabled(base::features::kPartitionAllocPCScan) ||
      base::FeatureList::IsEnabled(kPCScanBlinkPartitions)) {
    base::internal::PCScan::RegisterNonScannableRoot(array_buffer_root_);
#if !BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
    base::internal::PCScan::RegisterScannableRoot(fast_malloc_root_);
#endif
    base::internal::PCScan::RegisterScannableRoot(buffer_root_);
  }
#endif  // defined(PA_ALLOW_PCSCAN)

  initialized_ = true;
  return initialized_;
}

// static
void Partitions::StartPeriodicReclaim(
    scoped_refptr<base::SequencedTaskRunner> task_runner) {
  CHECK(IsMainThread());
  DCHECK(initialized_);

  base::PartitionAllocMemoryReclaimer::Instance()->Start(task_runner);
}

// static
void Partitions::DumpMemoryStats(
    bool is_light_dump,
    base::PartitionStatsDumper* partition_stats_dumper) {
  // Object model and rendering partitions are not thread safe and can be
  // accessed only on the main thread.
  DCHECK(IsMainThread());

#if !BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
  FastMallocPartition()->DumpStats("fast_malloc", is_light_dump,
                                   partition_stats_dumper);
#endif
  ArrayBufferPartition()->DumpStats("array_buffer", is_light_dump,
                                    partition_stats_dumper);
  BufferPartition()->DumpStats("buffer", is_light_dump, partition_stats_dumper);
  LayoutPartition()->DumpStats("layout", is_light_dump, partition_stats_dumper);
}

namespace {

class LightPartitionStatsDumperImpl : public base::PartitionStatsDumper {
 public:
  LightPartitionStatsDumperImpl() : total_active_bytes_(0) {}

  void PartitionDumpTotals(
      const char* partition_name,
      const base::PartitionMemoryStats* memory_stats) override {
    total_active_bytes_ += memory_stats->total_active_bytes;
  }

  void PartitionsDumpBucketStats(
      const char* partition_name,
      const base::PartitionBucketMemoryStats*) override {}

  size_t TotalActiveBytes() const { return total_active_bytes_; }

 private:
  size_t total_active_bytes_;
};

}  // namespace

// static
size_t Partitions::TotalSizeOfCommittedPages() {
  DCHECK(initialized_);
  size_t total_size = 0;
  // Racy reads below: this is fine to collect statistics.
#if !BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
  total_size +=
      TS_UNCHECKED_READ(FastMallocPartition()->total_size_of_committed_pages);
#endif
  total_size +=
      TS_UNCHECKED_READ(ArrayBufferPartition()->total_size_of_committed_pages);
  total_size +=
      TS_UNCHECKED_READ(BufferPartition()->total_size_of_committed_pages);
  total_size +=
      TS_UNCHECKED_READ(LayoutPartition()->total_size_of_committed_pages);
  return total_size;
}

// static
size_t Partitions::TotalActiveBytes() {
  LightPartitionStatsDumperImpl dumper;
  WTF::Partitions::DumpMemoryStats(true, &dumper);
  return dumper.TotalActiveBytes();
}

static NOINLINE void PartitionsOutOfMemoryUsing2G(size_t size) {
  NO_CODE_FOLDING();
  size_t signature = 2UL * 1024 * 1024 * 1024;
  base::debug::Alias(&signature);
  OOM_CRASH(size);
}

static NOINLINE void PartitionsOutOfMemoryUsing1G(size_t size) {
  NO_CODE_FOLDING();
  size_t signature = 1UL * 1024 * 1024 * 1024;
  base::debug::Alias(&signature);
  OOM_CRASH(size);
}

static NOINLINE void PartitionsOutOfMemoryUsing512M(size_t size) {
  NO_CODE_FOLDING();
  size_t signature = 512 * 1024 * 1024;
  base::debug::Alias(&signature);
  OOM_CRASH(size);
}

static NOINLINE void PartitionsOutOfMemoryUsing256M(size_t size) {
  NO_CODE_FOLDING();
  size_t signature = 256 * 1024 * 1024;
  base::debug::Alias(&signature);
  OOM_CRASH(size);
}

static NOINLINE void PartitionsOutOfMemoryUsing128M(size_t size) {
  NO_CODE_FOLDING();
  size_t signature = 128 * 1024 * 1024;
  base::debug::Alias(&signature);
  OOM_CRASH(size);
}

static NOINLINE void PartitionsOutOfMemoryUsing64M(size_t size) {
  NO_CODE_FOLDING();
  size_t signature = 64 * 1024 * 1024;
  base::debug::Alias(&signature);
  OOM_CRASH(size);
}

static NOINLINE void PartitionsOutOfMemoryUsing32M(size_t size) {
  NO_CODE_FOLDING();
  size_t signature = 32 * 1024 * 1024;
  base::debug::Alias(&signature);
  OOM_CRASH(size);
}

static NOINLINE void PartitionsOutOfMemoryUsing16M(size_t size) {
  NO_CODE_FOLDING();
  size_t signature = 16 * 1024 * 1024;
  base::debug::Alias(&signature);
  OOM_CRASH(size);
}

static NOINLINE void PartitionsOutOfMemoryUsingLessThan16M(size_t size) {
  NO_CODE_FOLDING();
  size_t signature = 16 * 1024 * 1024 - 1;
  base::debug::Alias(&signature);
  OOM_CRASH(size);
}

// static
void* Partitions::BufferMalloc(size_t n, const char* type_name) {
  return BufferPartition()->Alloc(n, type_name);
}

// static
void* Partitions::BufferTryRealloc(void* p, size_t n, const char* type_name) {
  return BufferPartition()->TryRealloc(p, n, type_name);
}

// static
void Partitions::BufferFree(void* p) {
  BufferPartition()->Free(p);
}

// static
size_t Partitions::BufferPotentialCapacity(size_t n) {
  return BufferPartition()->AllocationCapacityFromRequestedSize(n);
}

// Ideally this would be removed when PartitionAlloc is malloc(), but there are
// quite a few callers. Just forward to the C functions instead.  Most of the
// usual callers will never reach here though, as USING_FAST_MALLOC() becomes a
// no-op.
// static
void* Partitions::FastMalloc(size_t n, const char* type_name) {
#if !BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
  return FastMallocPartition()->Alloc(n, type_name);
#else
  return malloc(n);
#endif
}

// static
void* Partitions::FastZeroedMalloc(size_t n, const char* type_name) {
#if !BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
  return FastMallocPartition()->AllocFlags(base::PartitionAllocZeroFill, n,
                                           type_name);
#else
  return calloc(n, 1);
#endif
}

// static
void Partitions::FastFree(void* p) {
#if !BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
  FastMallocPartition()->Free(p);
#else
  free(p);
#endif
}

// static
void Partitions::HandleOutOfMemory(size_t size) {
  volatile size_t total_usage = TotalSizeOfCommittedPages();
  uint32_t alloc_page_error_code = base::GetAllocPageErrorCode();
  base::debug::Alias(&alloc_page_error_code);

  // Report the total mapped size from PageAllocator. This is intended to
  // distinguish better between address space exhaustion and out of memory on 32
  // bit platforms. PartitionAlloc can use a lot of address space, as free pages
  // are not shared between buckets (see crbug.com/421387). There is already
  // reporting for this, however it only looks at the address space usage of a
  // single partition. This allows to look across all the partitions, and other
  // users such as V8.
  char value[24];
  // %d works for 64 bit types as well with SafeSPrintf(), see its unit tests
  // for an example.
  base::strings::SafeSPrintf(value, "%d", base::GetTotalMappedSize());
  static crash_reporter::CrashKeyString<24> g_page_allocator_mapped_size(
      "page-allocator-mapped-size");
  g_page_allocator_mapped_size.Set(value);

  if (total_usage >= 2UL * 1024 * 1024 * 1024)
    PartitionsOutOfMemoryUsing2G(size);
  if (total_usage >= 1UL * 1024 * 1024 * 1024)
    PartitionsOutOfMemoryUsing1G(size);
  if (total_usage >= 512 * 1024 * 1024)
    PartitionsOutOfMemoryUsing512M(size);
  if (total_usage >= 256 * 1024 * 1024)
    PartitionsOutOfMemoryUsing256M(size);
  if (total_usage >= 128 * 1024 * 1024)
    PartitionsOutOfMemoryUsing128M(size);
  if (total_usage >= 64 * 1024 * 1024)
    PartitionsOutOfMemoryUsing64M(size);
  if (total_usage >= 32 * 1024 * 1024)
    PartitionsOutOfMemoryUsing32M(size);
  if (total_usage >= 16 * 1024 * 1024)
    PartitionsOutOfMemoryUsing16M(size);
  PartitionsOutOfMemoryUsingLessThan16M(size);
}

}  // namespace WTF

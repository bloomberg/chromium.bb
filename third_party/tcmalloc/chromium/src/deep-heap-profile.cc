// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// ---
// Author: Sainbayar Sukhbaatar
//         Dai Mikurube
//

#include "deep-heap-profile.h"

#ifdef DEEP_HEAP_PROFILE
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>  // for getpagesize and getpid
#endif  // HAVE_UNISTD_H

#include "base/cycleclock.h"
#include "base/sysinfo.h"

static const int kProfilerBufferSize = 1 << 20;
static const int kHashTableSize = 179999;  // The same as heap-profile-table.cc.

static const int PAGEMAP_BYTES = 8;
static const uint64 MAX_ADDRESS = kuint64max;

// Header strings of the dumped heap profile.
static const char kProfileHeader[] = "heap profile: ";
static const char kProfileVersion[] = "DUMP_DEEP_3";
static const char kGlobalStatsHeader[] = "GLOBAL_STATS:\n";
static const char kMMapStacktraceHeader[] = "MMAP_STACKTRACES:\n";
static const char kAllocStacktraceHeader[] = "MALLOC_STACKTRACES:\n";
static const char kProcSelfMapsHeader[] = "\nMAPPED_LIBRARIES:\n";

static const char kVirtualLabel[] = "virtual";
static const char kCommittedLabel[] = "committed";

DeepHeapProfile::DeepHeapProfile(HeapProfileTable* heap_profile,
                                 const char* prefix)
    : pagemap_fd_(-1),
      most_recent_pid_(-1),
      stats_(),
      dump_count_(0),
      filename_prefix_(NULL),
      profiler_buffer_(NULL),
      bucket_id_(0),
      heap_profile_(heap_profile) {
  deep_bucket_map_ = new(heap_profile_->alloc_(sizeof(DeepBucketMap)))
      DeepBucketMap(heap_profile_->alloc_, heap_profile_->dealloc_);

  // Copy filename prefix.
  const int prefix_length = strlen(prefix);
  filename_prefix_ =
      reinterpret_cast<char*>(heap_profile_->alloc_(prefix_length + 1));
  memcpy(filename_prefix_, prefix, prefix_length);
  filename_prefix_[prefix_length] = '\0';

  profiler_buffer_ =
      reinterpret_cast<char*>(heap_profile_->alloc_(kProfilerBufferSize));
}

DeepHeapProfile::~DeepHeapProfile() {
  heap_profile_->dealloc_(profiler_buffer_);
  heap_profile_->dealloc_(filename_prefix_);
  deep_bucket_map_->~DeepBucketMap();
  heap_profile_->dealloc_(deep_bucket_map_);
}

int DeepHeapProfile::FillOrderedProfile(char buffer[], int buffer_size) {
#ifndef NDEBUG
  int64 starting_cycles = CycleClock::Now();
#endif
  ++dump_count_;

  // Re-open files in /proc/pid/ if the process is newly forked one.
  if (most_recent_pid_ != getpid()) {
    most_recent_pid_ = getpid();
    pagemap_fd_ = OpenProcPagemap();

    deep_bucket_map_->Iterate(ClearIsLogged, this);

    // Write maps into a .maps file with using the global buffer.
    WriteMapsToFile(filename_prefix_, kProfilerBufferSize, profiler_buffer_);
  }

  // Reset committed sizes of buckets.
  ResetCommittedSize(heap_profile_->alloc_table_);
  ResetCommittedSize(heap_profile_->mmap_table_);

  SnapshotGlobalStatsWithoutMalloc(pagemap_fd_, &stats_);
  size_t anonymous_committed = stats_.anonymous.committed_bytes();

  // Note: Try to minimize the number of calls to malloc in the following
  // region, up until we call WriteBucketsToBucketFile(), near the end of this
  // function.  Calling malloc in the region may make a gap between the
  // observed size and actual memory allocation.  The gap is less than or equal
  // to the size of allocated memory in the region.  Calls to malloc won't
  // break anything, but can add some noise to the recorded information.
  // TODO(dmikurube): Eliminate dynamic memory allocation caused by snprintf.
  // glibc's snprintf internally allocates memory by alloca normally, but it
  // allocates memory by malloc if large memory is required.

  // Record committed sizes.
  SnapshotAllAllocsWithoutMalloc();

  // Check if committed bytes changed during SnapshotAllAllocsWithoutMalloc.
  SnapshotGlobalStatsWithoutMalloc(pagemap_fd_, &stats_);
#ifndef NDEBUG
  size_t committed_difference =
      stats_.anonymous.committed_bytes() - anonymous_committed;
  if (committed_difference != 0) {
    RAW_LOG(0, "Difference in committed size: %ld", committed_difference);
  }
#endif

  // Start filling buffer with the ordered profile.
  int printed = snprintf(buffer, buffer_size,
                         "%s%s\n", kProfileHeader, kProfileVersion);
  if (IsPrintedStringValid(printed, buffer_size, 0)) {
    return 0;
  }
  int used_in_buffer = printed;

  // Fill buffer with the global stats.
  printed = snprintf(buffer + used_in_buffer, buffer_size - used_in_buffer,
                     kGlobalStatsHeader);
  if (IsPrintedStringValid(printed, buffer_size, used_in_buffer)) {
    return used_in_buffer;
  }
  used_in_buffer += printed;

  used_in_buffer = UnparseGlobalStats(used_in_buffer, buffer_size, buffer);

  // Fill buffer with the header for buckets of mmap'ed regions.
  printed = snprintf(buffer + used_in_buffer, buffer_size - used_in_buffer,
                     kMMapStacktraceHeader);
  if (IsPrintedStringValid(printed, buffer_size, used_in_buffer)) {
    return used_in_buffer;
  }
  used_in_buffer += printed;

  printed = snprintf(buffer + used_in_buffer, buffer_size - used_in_buffer,
                     "%10s %10s\n", kVirtualLabel, kCommittedLabel);
  if (IsPrintedStringValid(printed, buffer_size, used_in_buffer)) {
    return used_in_buffer;
  }
  used_in_buffer += printed;

  // Fill buffer with stack trace buckets of mmap'ed regions.
  used_in_buffer = SnapshotBucketTableWithoutMalloc(heap_profile_->mmap_table_,
                                                    used_in_buffer,
                                                    buffer_size,
                                                    buffer);

  // Fill buffer with the header for buckets of allocated regions.
  printed = snprintf(buffer + used_in_buffer, buffer_size - used_in_buffer,
                     kAllocStacktraceHeader);
  if (IsPrintedStringValid(printed, buffer_size, used_in_buffer)) {
    return used_in_buffer;
  }
  used_in_buffer += printed;

  printed = snprintf(buffer + used_in_buffer, buffer_size - used_in_buffer,
                     "%10s %10s\n", kVirtualLabel, kCommittedLabel);
  if (IsPrintedStringValid(printed, buffer_size, used_in_buffer)) {
    return used_in_buffer;
  }
  used_in_buffer += printed;

  // Fill buffer with stack trace buckets of allocated regions.
  used_in_buffer = SnapshotBucketTableWithoutMalloc(heap_profile_->alloc_table_,
                                                    used_in_buffer,
                                                    buffer_size,
                                                    buffer);

  RAW_DCHECK(used_in_buffer < buffer_size, "");

  // Note: Memory snapshots are complete, and malloc may again be used freely.

  // Write the bucket listing into a .bucket file.
  WriteBucketsToBucketFile();

#ifndef NDEBUG
  int64 elapsed_cycles = CycleClock::Now() - starting_cycles;
  double elapsed_seconds = elapsed_cycles / CyclesPerSecond();
  RAW_LOG(0, "Time spent on DeepProfiler: %.3f sec\n", elapsed_seconds);
#endif

  return used_in_buffer;
}

void DeepHeapProfile::RegionStats::Initialize() {
  virtual_bytes_ = 0;
  committed_bytes_ = 0;
}

void DeepHeapProfile::RegionStats::Record(
    int pagemap_fd, uint64 first_address, uint64 last_address) {
  virtual_bytes_ += static_cast<size_t>(last_address - first_address + 1);
  committed_bytes_ += GetCommittedSize(pagemap_fd, first_address, last_address);
}

// static
bool DeepHeapProfile::IsPrintedStringValid(int printed,
                                           int buffer_size,
                                           int used_in_buffer) {
  return printed < 0 || printed >= buffer_size - used_in_buffer;
}

// TODO(dmikurube): Avoid calling ClearIsLogged to rewrite buckets by add a
// reference to a previous file in a .heap file.
// static
void DeepHeapProfile::ClearIsLogged(const void* pointer,
                                    DeepHeapProfile::DeepBucket* deep_bucket,
                                    DeepHeapProfile* deep_profile) {
  deep_bucket->is_logged = false;
}

// static
int DeepHeapProfile::OpenProcPagemap() {
  char filename[100];
  snprintf(filename, sizeof(filename), "/proc/%d/pagemap",
           static_cast<int>(getpid()));
  int pagemap_fd = open(filename, O_RDONLY);
  RAW_DCHECK(pagemap_fd != -1, "Failed to open /proc/self/pagemap");
  return pagemap_fd;
}

// static
bool DeepHeapProfile::SeekProcPagemap(int pagemap_fd, uint64 address) {
  int64 index = (address / getpagesize()) * PAGEMAP_BYTES;
  int64 offset = lseek64(pagemap_fd, index, SEEK_SET);
  RAW_DCHECK(offset == index, "Failed in seeking.");
  return offset >= 0;
}

// static
bool DeepHeapProfile::ReadProcPagemap(int pagemap_fd, PageState* state) {
  static const uint64 U64_1 = 1;
  static const uint64 PFN_FILTER = (U64_1 << 55) - U64_1;
  static const uint64 PAGE_PRESENT = U64_1 << 63;
  static const uint64 PAGE_SWAP = U64_1 << 62;
  static const uint64 PAGE_RESERVED = U64_1 << 61;
  static const uint64 FLAG_NOPAGE = U64_1 << 20;
  static const uint64 FLAG_KSM = U64_1 << 21;
  static const uint64 FLAG_MMAP = U64_1 << 11;

  uint64 pagemap_value;
  int result = read(pagemap_fd, &pagemap_value, PAGEMAP_BYTES);
  if (result != PAGEMAP_BYTES) {
    return false;
  }

  // Check if the page is committed.
  state->is_committed = (pagemap_value & (PAGE_PRESENT | PAGE_SWAP));

  state->is_present = (pagemap_value & PAGE_PRESENT);
  state->is_swapped = (pagemap_value & PAGE_SWAP);
  state->is_shared = false;

  return true;
}

// static
size_t DeepHeapProfile::GetCommittedSize(
    int pagemap_fd, uint64 first_address, uint64 last_address) {
  int page_size = getpagesize();
  uint64 page_address = (first_address / page_size) * page_size;
  size_t committed_size = 0;

  SeekProcPagemap(pagemap_fd, first_address);

  // Check every page on which the allocation resides.
  while (page_address <= last_address) {
    // Read corresponding physical page.
    PageState state;
    if (ReadProcPagemap(pagemap_fd, &state) == false) {
      // We can't read the last region (e.g vsyscall).
#ifndef NDEBUG
      RAW_LOG(0, "pagemap read failed @ %#llx %"PRId64" bytes",
              first_address, last_address - first_address + 1);
#endif
      return 0;
    }

    if (state.is_committed) {
      // Calculate the size of the allocation part in this page.
      size_t bytes = page_size;

      // If looking at the last page in a given region.
      if (last_address <= page_address - 1 + page_size) {
        bytes = last_address - page_address + 1;
      }

      // If looking at the first page in a given region.
      if (page_address < first_address) {
        bytes -= first_address - page_address;
      }

      committed_size += bytes;
    }
    if (page_address > MAX_ADDRESS - page_size) {
      break;
    }
    page_address += page_size;
  }

  return committed_size;
}

// static
void DeepHeapProfile::WriteMapsToFile(const char* filename_prefix,
                                      int buffer_size,
                                      char buffer[]) {
  char filename[100];
  snprintf(filename, sizeof(filename),
           "%s.%05d.maps", filename_prefix, static_cast<int>(getpid()));

  RawFD maps_fd = RawOpenForWriting(filename);
  RAW_DCHECK(maps_fd != kIllegalRawFD, "");

  int map_length;
  bool wrote_all;
  map_length = tcmalloc::FillProcSelfMaps(buffer, buffer_size, &wrote_all);
  RAW_DCHECK(wrote_all, "");
  RAW_DCHECK(map_length <= buffer_size, "");
  RawWrite(maps_fd, buffer, map_length);
  RawClose(maps_fd);
}

// TODO(dmikurube): Eliminate dynamic memory allocation caused by snprintf.
// ProcMapsIterator uses snprintf internally in construction.
// static
void DeepHeapProfile::SnapshotGlobalStatsWithoutMalloc(int pagemap_fd,
                                                       GlobalStats* stats) {
  ProcMapsIterator::Buffer iterator_buffer;
  ProcMapsIterator iterator(0, &iterator_buffer);
  uint64 first_address, last_address, offset;
  int64 unused_inode;
  char* flags;
  char* filename;

  stats->total.Initialize();
  stats->file_mapped.Initialize();
  stats->anonymous.Initialize();
  stats->other.Initialize();

  while (iterator.Next(&first_address, &last_address,
                       &flags, &offset, &unused_inode, &filename)) {
    // 'last_address' should be the last inclusive address of the region.
    last_address -= 1;
    if (strcmp("[vsyscall]", filename) == 0) {
      continue;  // Reading pagemap will fail in [vsyscall].
    }

    stats->total.Record(pagemap_fd, first_address, last_address);

    if (filename[0] == '/') {
      stats->file_mapped.Record(pagemap_fd, first_address, last_address);
    } else if (filename[0] == '\0' || filename[0] == '\n') {
      stats->anonymous.Record(pagemap_fd, first_address, last_address);
    } else {
      stats->other.Record(pagemap_fd, first_address, last_address);
    }
  }
}

DeepHeapProfile::DeepBucket* DeepHeapProfile::GetDeepBucket(Bucket* bucket) {
  DeepBucket* found = deep_bucket_map_->FindMutable(bucket);
  if (found != NULL)
    return found;

  DeepBucket created;
  created.bucket = bucket;
  created.committed_size = 0;
  created.id = (bucket_id_++);
  created.is_logged = false;
  deep_bucket_map_->Insert(bucket, created);
  return deep_bucket_map_->FindMutable(bucket);
}

void DeepHeapProfile::ResetCommittedSize(Bucket** bucket_table) {
  for (int i = 0; i < kHashTableSize; i++) {
    for (Bucket* bucket = bucket_table[i];
         bucket != NULL;
         bucket = bucket->next) {
      DeepBucket* deep_bucket = GetDeepBucket(bucket);
      deep_bucket->committed_size = 0;
    }
  }
}

// TODO(dmikurube): Eliminate dynamic memory allocation caused by snprintf.
int DeepHeapProfile::SnapshotBucketTableWithoutMalloc(Bucket** bucket_table,
                                                      int used_in_buffer,
                                                      int buffer_size,
                                                      char buffer[]) {
  for (int i = 0; i < kHashTableSize; i++) {
    for (Bucket* bucket = bucket_table[i];
         bucket != NULL;
         bucket = bucket->next) {
      if (bucket->alloc_size - bucket->free_size == 0) {
        continue;  // Skip empty buckets.
      }
      const DeepBucket* deep_bucket = GetDeepBucket(bucket);
      used_in_buffer = UnparseBucket(
          *deep_bucket, "", used_in_buffer, buffer_size, buffer, NULL);
    }
  }
  return used_in_buffer;
}

void DeepHeapProfile::RecordAlloc(const void* pointer,
                                  AllocValue* alloc_value,
                                  DeepHeapProfile* deep_profile) {
  uint64 address = reinterpret_cast<uintptr_t>(pointer);
  size_t committed = GetCommittedSize(deep_profile->pagemap_fd_,
      address, address + alloc_value->bytes - 1);

  DeepBucket* deep_bucket = deep_profile->GetDeepBucket(alloc_value->bucket());
  deep_bucket->committed_size += committed;
  deep_profile->stats_.record_malloc.AddToVirtualBytes(alloc_value->bytes);
  deep_profile->stats_.record_malloc.AddToCommittedBytes(committed);
}

void DeepHeapProfile::RecordMMap(const void* pointer,
                                 AllocValue* alloc_value,
                                 DeepHeapProfile* deep_profile) {
  uint64 address = reinterpret_cast<uintptr_t>(pointer);
  size_t committed = GetCommittedSize(deep_profile->pagemap_fd_,
      address, address + alloc_value->bytes - 1);

  DeepBucket* deep_bucket = deep_profile->GetDeepBucket(alloc_value->bucket());
  deep_bucket->committed_size += committed;
  deep_profile->stats_.record_mmap.AddToVirtualBytes(alloc_value->bytes);
  deep_profile->stats_.record_mmap.AddToCommittedBytes(committed);
}

void DeepHeapProfile::SnapshotAllAllocsWithoutMalloc() {
  stats_.record_mmap.Initialize();
  stats_.record_malloc.Initialize();

  // malloc allocations.
  heap_profile_->alloc_address_map_->Iterate(RecordAlloc, this);

  // mmap allocations.
  heap_profile_->mmap_address_map_->Iterate(RecordMMap, this);
}

int DeepHeapProfile::FillBucketForBucketFile(const DeepBucket* deep_bucket,
                                             int buffer_size,
                                             char buffer[]) {
  const Bucket* bucket = deep_bucket->bucket;
  int printed = snprintf(buffer, buffer_size, "%05d", deep_bucket->id);
  if (IsPrintedStringValid(printed, buffer_size, 0)) {
    return 0;
  }
  int used_in_buffer = printed;

  for (int depth = 0; depth < bucket->depth; depth++) {
    printed = snprintf(buffer + used_in_buffer, buffer_size - used_in_buffer,
                       " 0x%08" PRIxPTR,
                       reinterpret_cast<uintptr_t>(bucket->stack[depth]));
    if (IsPrintedStringValid(printed, buffer_size, used_in_buffer)) {
      return used_in_buffer;
    }
    used_in_buffer += printed;
  }
  printed = snprintf(buffer + used_in_buffer, buffer_size - used_in_buffer,
                     "\n");
  if (IsPrintedStringValid(printed, buffer_size, used_in_buffer)) {
    return used_in_buffer;
  }
  used_in_buffer += printed;

  return used_in_buffer;
}

void DeepHeapProfile::WriteBucketsTableToBucketFile(Bucket** bucket_table,
                                                    RawFD bucket_fd) {
  // We will use the global buffer here.
  char* buffer = profiler_buffer_;
  int buffer_size = kProfilerBufferSize;
  int used_in_buffer = 0;

  for (int i = 0; i < kHashTableSize; i++) {
    for (Bucket* bucket = bucket_table[i];
         bucket != NULL;
         bucket = bucket->next) {
      DeepBucket* deep_bucket = GetDeepBucket(bucket);
      if (deep_bucket->is_logged) {
        continue;  // Skip the bucket if it is already logged.
      }
      if (bucket->alloc_size - bucket->free_size <= 64) {
        continue;  // Skip small buckets.
      }

      used_in_buffer += FillBucketForBucketFile(
          deep_bucket, buffer_size - used_in_buffer, buffer + used_in_buffer);
      deep_bucket->is_logged = true;

      // Write to file if buffer 80% full.
      if (used_in_buffer > buffer_size * 0.8) {
        RawWrite(bucket_fd, buffer, used_in_buffer);
        used_in_buffer = 0;
      }
    }
  }

  RawWrite(bucket_fd, buffer, used_in_buffer);
}

void DeepHeapProfile::WriteBucketsToBucketFile() {
  char filename[100];
  snprintf(filename, sizeof(filename),
           "%s.%05d.%04d.buckets", filename_prefix_, getpid(), dump_count_);
  RawFD bucket_fd = RawOpenForWriting(filename);
  RAW_DCHECK(bucket_fd != kIllegalRawFD, "");

  WriteBucketsTableToBucketFile(heap_profile_->alloc_table_, bucket_fd);
  WriteBucketsTableToBucketFile(heap_profile_->mmap_table_, bucket_fd);

  RawClose(bucket_fd);
}

int DeepHeapProfile::UnparseBucket(const DeepBucket& deep_bucket,
                                   const char* extra,
                                   int used_in_buffer,
                                   int buffer_size,
                                   char* buffer,
                                   Stats* profile_stats) {
  const Bucket& bucket = *deep_bucket.bucket;
  if (profile_stats != NULL) {
    profile_stats->allocs += bucket.allocs;
    profile_stats->alloc_size += bucket.alloc_size;
    profile_stats->frees += bucket.frees;
    profile_stats->free_size += bucket.free_size;
  }

  int printed = snprintf(buffer + used_in_buffer, buffer_size - used_in_buffer,
                         "%10"PRId64" %10"PRId64" %6d %6d @%s %d\n",
                         bucket.alloc_size - bucket.free_size,
                         deep_bucket.committed_size,
                         bucket.allocs, bucket.frees, extra, deep_bucket.id);
  // If it looks like the snprintf failed, ignore the fact we printed anything.
  if (IsPrintedStringValid(printed, buffer_size, used_in_buffer)) {
    return used_in_buffer;
  }
  used_in_buffer += printed;

  return used_in_buffer;
}

int DeepHeapProfile::UnparseRegionStats(const RegionStats* stats,
                                        const char* name,
                                        int used_in_buffer,
                                        int buffer_size,
                                        char* buffer) {
  int printed = snprintf(buffer + used_in_buffer, buffer_size - used_in_buffer,
                         "%15s %10ld %10ld\n",
                         name, stats->virtual_bytes(),
                         stats->committed_bytes());
  if (IsPrintedStringValid(printed, buffer_size, used_in_buffer)) {
    return used_in_buffer;
  }
  used_in_buffer += printed;

  return used_in_buffer;
}

int DeepHeapProfile::UnparseGlobalStats(int used_in_buffer,
                                        int buffer_size,
                                        char* buffer) {
  int printed = snprintf(buffer + used_in_buffer, buffer_size - used_in_buffer,
                         "%15s %10s %10s\n", "",
                         kVirtualLabel, kCommittedLabel);
  if (IsPrintedStringValid(printed, buffer_size, used_in_buffer)) {
    return used_in_buffer;
  }
  used_in_buffer += printed;

  used_in_buffer = UnparseRegionStats(&(stats_.total), "total",
      used_in_buffer, buffer_size, buffer);
  used_in_buffer = UnparseRegionStats(&(stats_.file_mapped), "file mapped",
      used_in_buffer, buffer_size, buffer);
  used_in_buffer = UnparseRegionStats(&(stats_.anonymous), "anonymous",
      used_in_buffer, buffer_size, buffer);
  used_in_buffer = UnparseRegionStats(&(stats_.other), "other",
      used_in_buffer, buffer_size, buffer);
  used_in_buffer = UnparseRegionStats(&(stats_.record_mmap), "mmap",
      used_in_buffer, buffer_size, buffer);
  used_in_buffer = UnparseRegionStats(&(stats_.record_malloc), "tcmalloc",
      used_in_buffer, buffer_size, buffer);
  return used_in_buffer;
}
#else  // DEEP_HEAP_PROFILE

DeepHeapProfile::DeepHeapProfile(HeapProfileTable* heap_profile,
                                 const char* prefix)
    : heap_profile_(heap_profile) {
}

DeepHeapProfile::~DeepHeapProfile() {
}

int DeepHeapProfile::FillOrderedProfile(char buffer[], int buffer_size) {
  return heap_profile_->FillOrderedProfile(buffer, buffer_size);
}

#endif  // DEEP_HEAP_PROFILE

// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// ---
// Author: Sainbayar Sukhbaatar
//         Dai Mikurube
//

#include "deep-heap-profile.h"

#ifdef DEEP_HEAP_PROFILE
#include <algorithm>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>  // for getpagesize and getpid
#endif  // HAVE_UNISTD_H

#include "base/cycleclock.h"
#include "base/sysinfo.h"
#include "internal_logging.h"  // for ASSERT, etc

static const int kProfilerBufferSize = 1 << 20;
static const int kHashTableSize = 179999;  // The same as heap-profile-table.cc.

static const int PAGEMAP_BYTES = 8;
static const uint64 MAX_ADDRESS = kuint64max;

// Header strings of the dumped heap profile.
static const char kProfileHeader[] = "heap profile: ";
static const char kProfileVersion[] = "DUMP_DEEP_4";
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
  const int deep_table_bytes = kHashTableSize * sizeof(*deep_table_);
  deep_table_ = reinterpret_cast<DeepBucket**>(
      heap_profile->alloc_(deep_table_bytes));
  memset(deep_table_, 0, deep_table_bytes);

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
  DeallocateDeepBucketTable(deep_table_, heap_profile_);
  deep_table_ = NULL;
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

    for (int i = 0; i < kHashTableSize; i++) {
      for (DeepBucket* deep_bucket = deep_table_[i];
           deep_bucket != NULL;
           deep_bucket = deep_bucket->next) {
        deep_bucket->is_logged = false;
      }
    }

    // Write maps into a .maps file with using the global buffer.
    WriteMapsToFile(filename_prefix_, kProfilerBufferSize, profiler_buffer_);
  }

  // Reset committed sizes of buckets.
  ResetCommittedSize(heap_profile_->alloc_table_);
  ResetCommittedSize(heap_profile_->mmap_table_);

  // Allocate a list for mmap'ed regions.
  num_mmap_allocations_ = 0;
  heap_profile_->mmap_address_map_->Iterate(CountMMap, this);
  mmap_list_length_ = 0;
  mmap_list_ = reinterpret_cast<MMapListEntry*>(heap_profile_->alloc_(
      sizeof(MMapListEntry) * num_mmap_allocations_));

  // Touch all the allocated pages.  Touching is required to avoid new page
  // commitment while filling the list in SnapshotGlobalStatsWithoutMalloc.
  for (int i = 0;
       i < num_mmap_allocations_;
       i += getpagesize() / 2 / sizeof(MMapListEntry))
    mmap_list_[i].first_address = 0;
  mmap_list_[num_mmap_allocations_ - 1].last_address = 0;

  SnapshotGlobalStatsWithoutMalloc(pagemap_fd_, &stats_, NULL, 0);
  size_t anonymous_committed = stats_.all[ANONYMOUS].committed_bytes();

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
  SnapshotGlobalStatsWithoutMalloc(pagemap_fd_, &stats_,
                                   mmap_list_, mmap_list_length_);
#ifndef NDEBUG
  size_t committed_difference =
      stats_.all[ANONYMOUS].committed_bytes() - anonymous_committed;
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

  heap_profile_->dealloc_(mmap_list_);
  mmap_list_ = NULL;

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
void DeepHeapProfile::DeallocateDeepBucketTable(
    DeepBucket** deep_table, HeapProfileTable* heap_profile) {
  ASSERT(deep_table != NULL);
  for (int db = 0; db < kHashTableSize; db++) {
    for (DeepBucket* x = deep_table[db]; x != 0; /**/) {
      DeepBucket* db = x;
      x = x->next;
      heap_profile->dealloc_(db);
    }
  }
  heap_profile->dealloc_(deep_table);
}

// static
bool DeepHeapProfile::IsPrintedStringValid(int printed,
                                           int buffer_size,
                                           int used_in_buffer) {
  return printed < 0 || printed >= buffer_size - used_in_buffer;
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
    // TODO(dmikurube): Read pagemap in bulk for speed.
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
                                                       GlobalStats* stats,
                                                       MMapListEntry* mmap_list,
                                                       int mmap_list_length) {
  ProcMapsIterator::Buffer iterator_buffer;
  ProcMapsIterator iterator(0, &iterator_buffer);
  uint64 first_address, last_address, offset;
  int64 unused_inode;
  char* flags;
  char* filename;
  int mmap_list_index = 0;
  enum MapsRegionType type;

  for (int i = 0; i < NUMBER_OF_MAPS_REGION_TYPES; ++i) {
    stats->all[i].Initialize();
    stats->nonprofiled[i].Initialize();
  }

  while (iterator.Next(&first_address, &last_address,
                       &flags, &offset, &unused_inode, &filename)) {
    // 'last_address' should be the last inclusive address of the region.
    last_address -= 1;
    if (strcmp("[vsyscall]", filename) == 0) {
      continue;  // Reading pagemap will fail in [vsyscall].
    }

    type = ABSENT;
    if (filename[0] == '/') {
      if (flags[2] == 'x')
        type = FILE_EXEC;
      else
        type = FILE_NONEXEC;
    } else if (filename[0] == '\0' || filename[0] == '\n') {
      type = ANONYMOUS;
    } else if (strcmp(filename, "[stack]") == 0) {
      type = STACK;
    } else {
      type = OTHER;
    }
    stats->all[type].Record(pagemap_fd, first_address, last_address);

    // TODO(dmikurube): Avoid double-counting of pagemap.
    // Counts nonprofiled memory regions in /proc/<pid>/maps.
    if (mmap_list != NULL) {
      // It assumes that every mmap'ed region is included in one maps line.
      uint64 cursor = first_address;
      bool first = true;

      do {
        if (!first) {
          mmap_list[mmap_list_index].type = type;
          cursor = mmap_list[mmap_list_index].last_address + 1;
          ++mmap_list_index;
        }
        first = false;

        uint64 last_address_of_nonprofiled;
        // If the next mmap entry is away from the current maps line.
        if (mmap_list_index >= mmap_list_length ||
            mmap_list[mmap_list_index].first_address > last_address) {
          last_address_of_nonprofiled = last_address;
        } else {
          last_address_of_nonprofiled =
              mmap_list[mmap_list_index].first_address - 1;
        }

        if (last_address_of_nonprofiled + 1 > cursor) {
          stats->nonprofiled[type].Record(
              pagemap_fd, cursor, last_address_of_nonprofiled);
          cursor = last_address_of_nonprofiled + 1;
        }
      } while (mmap_list_index < mmap_list_length &&
               mmap_list[mmap_list_index].last_address <= last_address);
    }
  }
}

// GetDeepBucket is implemented as almost copy of heap-profile-table:GetBucket.
// It's to avoid modifying heap-profile-table.  Performance issues can be
// ignored in usual Chromium runs.  Another hash function can be tried in an
// easy way in future.
DeepHeapProfile::DeepBucket* DeepHeapProfile::GetDeepBucket(
    Bucket* bucket, DeepBucket **table) {
  // Make hash-value
  uintptr_t h = 0;

  h += reinterpret_cast<uintptr_t>(bucket);
  h += h << 10;
  h ^= h >> 6;
  // Repeat the three lines for other values if required.

  h += h << 3;
  h ^= h >> 11;

  // Lookup stack trace in table
  unsigned int buck = ((unsigned int) h) % kHashTableSize;
  for (DeepBucket* db = table[buck]; db != 0; db = db->next) {
    if (db->bucket == bucket) {
      return db;
    }
  }

  // Create new bucket
  DeepBucket* db =
      reinterpret_cast<DeepBucket*>(heap_profile_->alloc_(sizeof(DeepBucket)));
  memset(db, 0, sizeof(*db));
  db->bucket         = bucket;
  db->committed_size = 0;
  db->id             = (bucket_id_++);
  db->is_logged      = false;
  db->next           = table[buck];
  table[buck] = db;
  return db;
}

void DeepHeapProfile::ResetCommittedSize(Bucket** bucket_table) {
  for (int i = 0; i < kHashTableSize; i++) {
    for (Bucket* bucket = bucket_table[i];
         bucket != NULL;
         bucket = bucket->next) {
      DeepBucket* deep_bucket = GetDeepBucket(bucket, deep_table_);
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
      const DeepBucket* deep_bucket = GetDeepBucket(bucket, deep_table_);
      used_in_buffer = UnparseBucket(
          *deep_bucket, "", used_in_buffer, buffer_size, buffer, NULL);
    }
  }
  return used_in_buffer;
}

// static
bool DeepHeapProfile::ByFirstAddress(const MMapListEntry& a,
                                     const MMapListEntry& b) {
  return a.first_address < b.first_address;
}

// static
void DeepHeapProfile::CountMMap(const void* pointer,
                                AllocValue* alloc_value,
                                DeepHeapProfile* deep_profile) {
  ++deep_profile->num_mmap_allocations_;
}

void DeepHeapProfile::RecordAlloc(const void* pointer,
                                  AllocValue* alloc_value,
                                  DeepHeapProfile* deep_profile) {
  uint64 address = reinterpret_cast<uintptr_t>(pointer);
  size_t committed = GetCommittedSize(deep_profile->pagemap_fd_,
      address, address + alloc_value->bytes - 1);

  DeepBucket* deep_bucket = deep_profile->GetDeepBucket(
      alloc_value->bucket(), deep_profile->deep_table_);
  deep_bucket->committed_size += committed;
  deep_profile->stats_.profiled_malloc.AddToVirtualBytes(alloc_value->bytes);
  deep_profile->stats_.profiled_malloc.AddToCommittedBytes(committed);
}

void DeepHeapProfile::RecordMMap(const void* pointer,
                                 AllocValue* alloc_value,
                                 DeepHeapProfile* deep_profile) {
  uint64 address = reinterpret_cast<uintptr_t>(pointer);
  size_t committed = GetCommittedSize(deep_profile->pagemap_fd_,
      address, address + alloc_value->bytes - 1);

  DeepBucket* deep_bucket = deep_profile->GetDeepBucket(
      alloc_value->bucket(), deep_profile->deep_table_);
  deep_bucket->committed_size += committed;
  deep_profile->stats_.profiled_mmap.AddToVirtualBytes(alloc_value->bytes);
  deep_profile->stats_.profiled_mmap.AddToCommittedBytes(committed);

  if (deep_profile->mmap_list_length_ < deep_profile->num_mmap_allocations_) {
    deep_profile->mmap_list_[deep_profile->mmap_list_length_].first_address =
        address;
    deep_profile->mmap_list_[deep_profile->mmap_list_length_].last_address =
        address - 1 + alloc_value->bytes;
    deep_profile->mmap_list_[deep_profile->mmap_list_length_].type = ABSENT;
    ++deep_profile->mmap_list_length_;
  } else {
    RAW_LOG(0, "Unexpected number of mmap entries: %d/%d",
            deep_profile->mmap_list_length_,
            deep_profile->num_mmap_allocations_);
  }
}

void DeepHeapProfile::SnapshotAllAllocsWithoutMalloc() {
  stats_.profiled_mmap.Initialize();
  stats_.profiled_malloc.Initialize();

  // malloc allocations.
  heap_profile_->alloc_address_map_->Iterate(RecordAlloc, this);

  // mmap allocations.
  heap_profile_->mmap_address_map_->Iterate(RecordMMap, this);
  std::sort(mmap_list_, mmap_list_ + mmap_list_length_, ByFirstAddress);
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
      DeepBucket* deep_bucket = GetDeepBucket(bucket, deep_table_);
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
                         "%25s %12ld %12ld\n",
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
  RegionStats all_total;
  RegionStats nonprofiled_total;
  for (int i = 0; i < NUMBER_OF_MAPS_REGION_TYPES; ++i) {
    all_total.AddAnotherRegionStat(stats_.all[i]);
    nonprofiled_total.AddAnotherRegionStat(stats_.nonprofiled[i]);
  }
  int printed = snprintf(
      buffer + used_in_buffer, buffer_size - used_in_buffer,
      "# total (%lu) %c= profiled-mmap (%lu) + nonprofiled-* (%lu)\n",
      all_total.committed_bytes(),
      all_total.committed_bytes() ==
          stats_.profiled_mmap.committed_bytes() +
          nonprofiled_total.committed_bytes() ? '=' : '!',
      stats_.profiled_mmap.committed_bytes(),
      nonprofiled_total.committed_bytes());
  if (IsPrintedStringValid(printed, buffer_size, used_in_buffer)) {
    return used_in_buffer;
  }
  used_in_buffer += printed;

  printed = snprintf(buffer + used_in_buffer, buffer_size - used_in_buffer,
                         "%25s %12s %12s\n", "",
                         kVirtualLabel, kCommittedLabel);
  if (IsPrintedStringValid(printed, buffer_size, used_in_buffer)) {
    return used_in_buffer;
  }
  used_in_buffer += printed;

  used_in_buffer = UnparseRegionStats(&(all_total),
      "total", used_in_buffer, buffer_size, buffer);
  used_in_buffer = UnparseRegionStats(&(stats_.all[FILE_EXEC]),
      "file-exec", used_in_buffer, buffer_size, buffer);
  used_in_buffer = UnparseRegionStats(&(stats_.all[FILE_NONEXEC]),
      "file-nonexec", used_in_buffer, buffer_size, buffer);
  used_in_buffer = UnparseRegionStats(&(stats_.all[ANONYMOUS]),
      "anonymous", used_in_buffer, buffer_size, buffer);
  used_in_buffer = UnparseRegionStats(&(stats_.all[STACK]),
      "stack", used_in_buffer, buffer_size, buffer);
  used_in_buffer = UnparseRegionStats(&(stats_.all[OTHER]),
      "other", used_in_buffer, buffer_size, buffer);
  used_in_buffer = UnparseRegionStats(&(nonprofiled_total),
      "nonprofiled-total", used_in_buffer, buffer_size, buffer);
  used_in_buffer = UnparseRegionStats(&(stats_.nonprofiled[ABSENT]),
      "nonprofiled-absent", used_in_buffer, buffer_size, buffer);
  used_in_buffer = UnparseRegionStats(&(stats_.nonprofiled[ANONYMOUS]),
      "nonprofiled-anonymous", used_in_buffer, buffer_size, buffer);
  used_in_buffer = UnparseRegionStats(&(stats_.nonprofiled[FILE_EXEC]),
      "nonprofiled-file-exec", used_in_buffer, buffer_size, buffer);
  used_in_buffer = UnparseRegionStats(&(stats_.nonprofiled[FILE_NONEXEC]),
      "nonprofiled-file-nonexec", used_in_buffer, buffer_size, buffer);
  used_in_buffer = UnparseRegionStats(&(stats_.nonprofiled[STACK]),
      "nonprofiled-stack", used_in_buffer, buffer_size, buffer);
  used_in_buffer = UnparseRegionStats(&(stats_.nonprofiled[OTHER]),
      "nonprofiled-other", used_in_buffer, buffer_size, buffer);
  used_in_buffer = UnparseRegionStats(&(stats_.profiled_mmap),
      "profiled-mmap", used_in_buffer, buffer_size, buffer);
  used_in_buffer = UnparseRegionStats(&(stats_.profiled_malloc),
      "profiled-malloc", used_in_buffer, buffer_size, buffer);
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

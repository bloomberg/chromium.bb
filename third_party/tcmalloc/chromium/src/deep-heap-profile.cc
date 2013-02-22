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
static const int kHashTableSize = 179999;  // Same as heap-profile-table.cc.

static const int PAGEMAP_BYTES = 8;
static const uint64 MAX_ADDRESS = kuint64max;

// Tag strings in heap profile dumps.
static const char kProfileHeader[] = "heap profile: ";
static const char kProfileVersion[] = "DUMP_DEEP_6";
static const char kMMapListHeader[] = "MMAP_LIST:\n";
static const char kGlobalStatsHeader[] = "GLOBAL_STATS:\n";
static const char kStacktraceHeader[] = "STACKTRACES:\n";
static const char kProcSelfMapsHeader[] = "\nMAPPED_LIBRARIES:\n";

static const char kVirtualLabel[] = "virtual";
static const char kCommittedLabel[] = "committed";

const char* DeepHeapProfile::kMapsRegionTypeDict[] = {
  "absent",
  "anonymous",
  "file-exec",
  "file-nonexec",
  "stack",
  "other",
};

namespace {

#if defined(__linux__)

// Implements MemoryResidenceInfoGetterInterface for Linux.
class MemoryInfoGetterLinux :
    public DeepHeapProfile::MemoryResidenceInfoGetterInterface {
 public:
  MemoryInfoGetterLinux(): fd_(kIllegalRawFD) {}
  virtual ~MemoryInfoGetterLinux() {}

  // Opens /proc/<pid>/pagemap and stores its file descriptor.
  // It keeps open while the process is running.
  //
  // Note that file descriptors need to be refreshed after fork.
  virtual void Initialize();

  // Returns the number of resident (including swapped) bytes of the given
  // memory region from |first_address| to |last_address| inclusive.
  virtual size_t CommittedSize(uint64 first_address, uint64 last_address) const;

 private:
  struct State {
    bool is_committed;  // Currently, we use only this
    bool is_present;
    bool is_swapped;
    bool is_shared;
    bool is_mmap;
  };

  // Seeks to the offset of the open pagemap file.
  // It returns true if succeeded.
  bool Seek(uint64 address) const;

  // Reads a pagemap state from the current offset.
  // It returns true if succeeded.
  bool Read(State* state) const;

  RawFD fd_;
};

void MemoryInfoGetterLinux::Initialize() {
  char filename[100];
  snprintf(filename, sizeof(filename), "/proc/%d/pagemap",
           static_cast<int>(getpid()));
  fd_ = open(filename, O_RDONLY);
  RAW_DCHECK(fd_ != -1, "Failed to open /proc/self/pagemap");
}

size_t MemoryInfoGetterLinux::CommittedSize(
    uint64 first_address, uint64 last_address) const {
  int page_size = getpagesize();
  uint64 page_address = (first_address / page_size) * page_size;
  size_t committed_size = 0;

  Seek(first_address);

  // Check every page on which the allocation resides.
  while (page_address <= last_address) {
    // Read corresponding physical page.
    State state;
    // TODO(dmikurube): Read pagemap in bulk for speed.
    if (Read(&state) == false) {
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

bool MemoryInfoGetterLinux::Seek(uint64 address) const {
  int64 index = (address / getpagesize()) * PAGEMAP_BYTES;
  int64 offset = lseek64(fd_, index, SEEK_SET);
  RAW_DCHECK(offset == index, "Failed in seeking.");
  return offset >= 0;
}

bool MemoryInfoGetterLinux::Read(State* state) const {
  static const uint64 U64_1 = 1;
  static const uint64 PFN_FILTER = (U64_1 << 55) - U64_1;
  static const uint64 PAGE_PRESENT = U64_1 << 63;
  static const uint64 PAGE_SWAP = U64_1 << 62;
  static const uint64 PAGE_RESERVED = U64_1 << 61;
  static const uint64 FLAG_NOPAGE = U64_1 << 20;
  static const uint64 FLAG_KSM = U64_1 << 21;
  static const uint64 FLAG_MMAP = U64_1 << 11;

  uint64 pagemap_value;
  int result = read(fd_, &pagemap_value, PAGEMAP_BYTES);
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

#endif  // defined(__linux__)

}  // anonymous namespace

DeepHeapProfile::MemoryResidenceInfoGetterInterface::
    MemoryResidenceInfoGetterInterface() {}

DeepHeapProfile::MemoryResidenceInfoGetterInterface::
    ~MemoryResidenceInfoGetterInterface() {}

DeepHeapProfile::MemoryResidenceInfoGetterInterface*
    DeepHeapProfile::MemoryResidenceInfoGetterInterface::Create() {
#if defined(__linux__)
  return new MemoryInfoGetterLinux();
#else
  return NULL;
#endif
}

DeepHeapProfile::DeepHeapProfile(HeapProfileTable* heap_profile,
                                 const char* prefix)
    : memory_residence_info_getter_(
          MemoryResidenceInfoGetterInterface::Create()),
      most_recent_pid_(-1),
      stats_(),
      dump_count_(0),
      filename_prefix_(NULL),
      profiler_buffer_(NULL),
      deep_table_(kHashTableSize, heap_profile->alloc_, heap_profile->dealloc_),
      heap_profile_(heap_profile) {
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
  delete memory_residence_info_getter_;
}

// Global malloc() should not be used in this function.
// Use LowLevelAlloc if required.
int DeepHeapProfile::FillOrderedProfile(char raw_buffer[], int buffer_size) {
  TextBuffer buffer(raw_buffer, buffer_size);
  TextBuffer global_buffer(profiler_buffer_, kProfilerBufferSize);

#ifndef NDEBUG
  int64 starting_cycles = CycleClock::Now();
#endif
  ++dump_count_;

  // Re-open files in /proc/pid/ if the process is newly forked one.
  if (most_recent_pid_ != getpid()) {
    most_recent_pid_ = getpid();

    memory_residence_info_getter_->Initialize();
    deep_table_.ResetIsLogged();

    // Write maps into "|filename_prefix_|.<pid>.maps".
    WriteProcMaps(filename_prefix_, kProfilerBufferSize, profiler_buffer_);
  }

  // Reset committed sizes of buckets.
  deep_table_.ResetCommittedSize();

  // Allocate a list for mmap'ed regions.
  num_mmap_allocations_ = 0;
  if (heap_profile_->mmap_address_map_) {
    heap_profile_->mmap_address_map_->Iterate(CountMMap, this);

    mmap_list_length_ = 0;
    mmap_list_ = reinterpret_cast<MMapListEntry*>(heap_profile_->alloc_(
        sizeof(MMapListEntry) * num_mmap_allocations_));

    // Touch all the allocated pages.  Touching is required to avoid new page
    // commitment while filling the list in SnapshotProcMaps.
    for (int i = 0;
         i < num_mmap_allocations_;
         i += getpagesize() / 2 / sizeof(MMapListEntry))
      mmap_list_[i].first_address = 0;
    mmap_list_[num_mmap_allocations_ - 1].last_address = 0;
  }

  stats_.SnapshotProcMaps(memory_residence_info_getter_, NULL, 0, NULL);

  // TODO(dmikurube): Eliminate dynamic memory allocation caused by snprintf.
  // glibc's snprintf internally allocates memory by alloca normally, but it
  // allocates memory by malloc if large memory is required.

  // Record committed sizes.
  stats_.SnapshotAllocations(this);

  buffer.AppendString(kProfileHeader, 0);
  buffer.AppendString(kProfileVersion, 0);
  buffer.AppendString("\n", 0);

  // Fill buffer with the global stats.
  buffer.AppendString(kMMapListHeader, 0);

  // Check if committed bytes changed during SnapshotAllocations.
  stats_.SnapshotProcMaps(memory_residence_info_getter_,
                          mmap_list_,
                          mmap_list_length_,
                          &buffer);

  // Fill buffer with the global stats.
  buffer.AppendString(kGlobalStatsHeader, 0);

  stats_.Unparse(&buffer);

  buffer.AppendString(kStacktraceHeader, 0);
  buffer.AppendString(kVirtualLabel, 10);
  buffer.AppendChar(' ');
  buffer.AppendString(kCommittedLabel, 10);
  buffer.AppendString("\n", 0);

  // Fill buffer.
  deep_table_.UnparseForStats(&buffer);

  RAW_DCHECK(buffer.FilledBytes() < buffer_size, "");

  heap_profile_->dealloc_(mmap_list_);
  mmap_list_ = NULL;

  // Write the bucket listing into a .bucket file.
  deep_table_.WriteForBucketFile(filename_prefix_, dump_count_, &global_buffer);

#ifndef NDEBUG
  int64 elapsed_cycles = CycleClock::Now() - starting_cycles;
  double elapsed_seconds = elapsed_cycles / CyclesPerSecond();
  RAW_LOG(0, "Time spent on DeepProfiler: %.3f sec\n", elapsed_seconds);
#endif

  return buffer.FilledBytes();
}

int DeepHeapProfile::TextBuffer::Size() {
  return size_;
}

int DeepHeapProfile::TextBuffer::FilledBytes() {
  return cursor_;
}

void DeepHeapProfile::TextBuffer::Clear() {
  cursor_ = 0;
}

void DeepHeapProfile::TextBuffer::Write(RawFD fd) {
  RawWrite(fd, buffer_, cursor_);
}

// TODO(dmikurube): These Append* functions should not use snprintf.
bool DeepHeapProfile::TextBuffer::AppendChar(char v) {
  return ForwardCursor(snprintf(buffer_ + cursor_, size_ - cursor_, "%c", v));
}

bool DeepHeapProfile::TextBuffer::AppendString(const char* s, int d) {
  int appended;
  if (d == 0)
    appended = snprintf(buffer_ + cursor_, size_ - cursor_, "%s", s);
  else
    appended = snprintf(buffer_ + cursor_, size_ - cursor_, "%*s", d, s);
  return ForwardCursor(appended);
}

bool DeepHeapProfile::TextBuffer::AppendInt(int v, int d) {
  int appended;
  if (d == 0)
    appended = snprintf(buffer_ + cursor_, size_ - cursor_, "%d", v);
  else
    appended = snprintf(buffer_ + cursor_, size_ - cursor_, "%*d", d, v);
  return ForwardCursor(appended);
}

bool DeepHeapProfile::TextBuffer::AppendLong(long v, int d) {
  int appended;
  if (d == 0)
    appended = snprintf(buffer_ + cursor_, size_ - cursor_, "%ld", v);
  else
    appended = snprintf(buffer_ + cursor_, size_ - cursor_, "%*ld", d, v);
  return ForwardCursor(appended);
}

bool DeepHeapProfile::TextBuffer::AppendUnsignedLong(unsigned long v, int d) {
  int appended;
  if (d == 0)
    appended = snprintf(buffer_ + cursor_, size_ - cursor_, "%lu", v);
  else
    appended = snprintf(buffer_ + cursor_, size_ - cursor_, "%*lu", d, v);
  return ForwardCursor(appended);
}

bool DeepHeapProfile::TextBuffer::AppendInt64(int64 v, int d) {
  int appended;
  if (d == 0)
    appended = snprintf(buffer_ + cursor_, size_ - cursor_, "%"PRId64, v);
  else
    appended = snprintf(buffer_ + cursor_, size_ - cursor_, "%*"PRId64, d, v);
  return ForwardCursor(appended);
}

bool DeepHeapProfile::TextBuffer::AppendPtr(uint64 v, int d) {
  int appended;
  if (d == 0)
    appended = snprintf(buffer_ + cursor_, size_ - cursor_, "%"PRIxPTR, v);
  else
    appended = snprintf(buffer_ + cursor_, size_ - cursor_, "%0*"PRIxPTR, d, v);
  return ForwardCursor(appended);
}

bool DeepHeapProfile::TextBuffer::ForwardCursor(int appended) {
  if (appended < 0 || appended >= size_ - cursor_)
    return false;
  cursor_ += appended;
  return true;
}

void DeepHeapProfile::DeepBucket::UnparseForStats(TextBuffer* buffer) {
  buffer->AppendInt64(bucket->alloc_size - bucket->free_size, 10);
  buffer->AppendChar(' ');
  buffer->AppendInt64(committed_size, 10);
  buffer->AppendChar(' ');
  buffer->AppendInt(bucket->allocs, 6);
  buffer->AppendChar(' ');
  buffer->AppendInt(bucket->frees, 6);
  buffer->AppendString(" @ ", 0);
  buffer->AppendInt(id, 0);
  buffer->AppendString("\n", 0);
}

void DeepHeapProfile::DeepBucket::UnparseForBucketFile(TextBuffer* buffer) {
  buffer->AppendInt(id, 0);
  buffer->AppendChar(' ');
  buffer->AppendString(is_mmap ? "mmap" : "malloc", 0);

#if defined(TYPE_PROFILING)
  buffer->AppendString(" t0x", 0);
  buffer->AppendPtr(reinterpret_cast<uintptr_t>(type), 0);
  if (type == NULL) {
    buffer->AppendString(" nno_typeinfo", 0);
  } else {
    buffer->AppendString(" n", 0);
    buffer->AppendString(type->name(), 0);
  }
#endif

  for (int depth = 0; depth < bucket->depth; depth++) {
    buffer->AppendString(" 0x", 0);
    buffer->AppendPtr(reinterpret_cast<uintptr_t>(bucket->stack[depth]), 8);
  }
  buffer->AppendString("\n", 0);
}

DeepHeapProfile::DeepBucketTable::DeepBucketTable(
    int table_size,
    HeapProfileTable::Allocator alloc,
    HeapProfileTable::DeAllocator dealloc)
    : table_(NULL),
      table_size_(table_size),
      alloc_(alloc),
      dealloc_(dealloc),
      bucket_id_(0) {
  const int bytes = table_size * sizeof(DeepBucket*);
  table_ = reinterpret_cast<DeepBucket**>(alloc(bytes));
  memset(table_, 0, bytes);
}

DeepHeapProfile::DeepBucketTable::~DeepBucketTable() {
  ASSERT(table_ != NULL);
  for (int db = 0; db < table_size_; db++) {
    for (DeepBucket* x = table_[db]; x != 0; /**/) {
      DeepBucket* db = x;
      x = x->next;
      dealloc_(db);
    }
  }
  dealloc_(table_);
}

DeepHeapProfile::DeepBucket* DeepHeapProfile::DeepBucketTable::Lookup(
    Bucket* bucket,
#if defined(TYPE_PROFILING)
    const std::type_info* type,
#endif
    bool is_mmap) {
  // Make hash-value
  uintptr_t h = 0;

  AddToHashValue(reinterpret_cast<uintptr_t>(bucket), &h);
  if (is_mmap) {
    AddToHashValue(1, &h);
  } else {
    AddToHashValue(0, &h);
  }

#if defined(TYPE_PROFILING)
  if (type == NULL) {
    AddToHashValue(0, &h);
  } else {
    AddToHashValue(reinterpret_cast<uintptr_t>(type->name()), &h);
  }
#endif

  FinishHashValue(&h);

  // Lookup stack trace in table
  unsigned int buck = ((unsigned int) h) % table_size_;
  for (DeepBucket* db = table_[buck]; db != 0; db = db->next) {
    if (db->bucket == bucket) {
      return db;
    }
  }

  // Create a new bucket
  DeepBucket* db = reinterpret_cast<DeepBucket*>(alloc_(sizeof(DeepBucket)));
  memset(db, 0, sizeof(*db));
  db->bucket         = bucket;
#if defined(TYPE_PROFILING)
  db->type           = type;
#endif
  db->committed_size = 0;
  db->is_mmap        = is_mmap;
  db->id             = (bucket_id_++);
  db->is_logged      = false;
  db->next           = table_[buck];
  table_[buck] = db;
  return db;
}

// TODO(dmikurube): Eliminate dynamic memory allocation caused by snprintf.
void DeepHeapProfile::DeepBucketTable::UnparseForStats(TextBuffer* buffer) {
  for (int i = 0; i < table_size_; i++) {
    for (DeepBucket* deep_bucket = table_[i];
         deep_bucket != NULL;
         deep_bucket = deep_bucket->next) {
      Bucket* bucket = deep_bucket->bucket;
      if (bucket->alloc_size - bucket->free_size == 0) {
        continue;  // Skip empty buckets.
      }
      deep_bucket->UnparseForStats(buffer);
    }
  }
}

void DeepHeapProfile::DeepBucketTable::WriteForBucketFile(
    const char* prefix, int dump_count, TextBuffer* buffer) {
  char filename[100];
  snprintf(filename, sizeof(filename),
           "%s.%05d.%04d.buckets", prefix, getpid(), dump_count);
  RawFD fd = RawOpenForWriting(filename);
  RAW_DCHECK(fd != kIllegalRawFD, "");

  for (int i = 0; i < table_size_; i++) {
    for (DeepBucket* deep_bucket = table_[i];
         deep_bucket != NULL;
         deep_bucket = deep_bucket->next) {
      Bucket* bucket = deep_bucket->bucket;
      if (deep_bucket->is_logged) {
        continue;  // Skip the bucket if it is already logged.
      }
      if (bucket->alloc_size - bucket->free_size <= 64) {
        continue;  // Skip small buckets.
      }

      deep_bucket->UnparseForBucketFile(buffer);
      deep_bucket->is_logged = true;

      // Write to file if buffer 80% full.
      if (buffer->FilledBytes() > buffer->Size() * 0.8) {
        buffer->Write(fd);
        buffer->Clear();
      }
    }
  }

  buffer->Write(fd);
  RawClose(fd);
}

void DeepHeapProfile::DeepBucketTable::ResetCommittedSize() {
  for (int i = 0; i < table_size_; i++) {
    for (DeepBucket* deep_bucket = table_[i];
         deep_bucket != NULL;
         deep_bucket = deep_bucket->next) {
      deep_bucket->committed_size = 0;
    }
  }
}

void DeepHeapProfile::DeepBucketTable::ResetIsLogged() {
  for (int i = 0; i < table_size_; i++) {
    for (DeepBucket* deep_bucket = table_[i];
         deep_bucket != NULL;
         deep_bucket = deep_bucket->next) {
      deep_bucket->is_logged = false;
    }
  }
}

// This hash function is from HeapProfileTable::GetBucket.
// static
void DeepHeapProfile::DeepBucketTable::AddToHashValue(
    uintptr_t add, uintptr_t* hash_value) {
  *hash_value += add;
  *hash_value += *hash_value << 10;
  *hash_value ^= *hash_value >> 6;
}

// This hash function is from HeapProfileTable::GetBucket.
// static
void DeepHeapProfile::DeepBucketTable::FinishHashValue(uintptr_t* hash_value) {
  *hash_value += *hash_value << 3;
  *hash_value ^= *hash_value >> 11;
}

void DeepHeapProfile::RegionStats::Initialize() {
  virtual_bytes_ = 0;
  committed_bytes_ = 0;
}

uint64 DeepHeapProfile::RegionStats::Record(
    const MemoryResidenceInfoGetterInterface* memory_residence_info_getter,
    uint64 first_address,
    uint64 last_address) {
  uint64 committed;
  virtual_bytes_ += static_cast<size_t>(last_address - first_address + 1);
  committed = memory_residence_info_getter->CommittedSize(first_address,
                                                          last_address);
  committed_bytes_ += committed;
  return committed;
}

void DeepHeapProfile::RegionStats::Unparse(const char* name,
                                           TextBuffer* buffer) {
  buffer->AppendString(name, 25);
  buffer->AppendChar(' ');
  buffer->AppendLong(virtual_bytes_, 12);
  buffer->AppendChar(' ');
  buffer->AppendLong(committed_bytes_, 12);
  buffer->AppendString("\n", 0);
}

// TODO(dmikurube): Eliminate dynamic memory allocation caused by snprintf.
void DeepHeapProfile::GlobalStats::SnapshotProcMaps(
    const MemoryResidenceInfoGetterInterface* memory_residence_info_getter,
    MMapListEntry* mmap_list,
    int mmap_list_length,
    TextBuffer* mmap_dump_buffer) {
  ProcMapsIterator::Buffer iterator_buffer;
  ProcMapsIterator iterator(0, &iterator_buffer);
  uint64 first_address, last_address, offset;
  int64 inode;
  char* flags;
  char* filename;
  int mmap_list_index = 0;
  enum MapsRegionType type;

  for (int i = 0; i < NUMBER_OF_MAPS_REGION_TYPES; ++i) {
    all_[i].Initialize();
    unhooked_[i].Initialize();
  }

  while (iterator.Next(&first_address, &last_address,
                       &flags, &offset, &inode, &filename)) {
    if (mmap_dump_buffer) {
      char buffer[1024];
      int written = iterator.FormatLine(buffer, sizeof(buffer),
                                        first_address, last_address, flags,
                                        offset, inode, filename, 0);
      mmap_dump_buffer->AppendString(buffer, 0);
    }

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
    all_[type].Record(
        memory_residence_info_getter, first_address, last_address);

    // TODO(dmikurube): Stop double-counting pagemap.
    // Counts unhooked memory regions in /proc/<pid>/maps.
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

        uint64 last_address_of_unhooked;
        // If the next mmap entry is away from the current maps line.
        if (mmap_list_index >= mmap_list_length ||
            mmap_list[mmap_list_index].first_address > last_address) {
          last_address_of_unhooked = last_address;
        } else {
          last_address_of_unhooked =
              mmap_list[mmap_list_index].first_address - 1;
        }

        if (last_address_of_unhooked + 1 > cursor) {
          uint64 committed_size = unhooked_[type].Record(
              memory_residence_info_getter,
              cursor,
              last_address_of_unhooked);
          if (mmap_dump_buffer) {
            mmap_dump_buffer->AppendString("  ", 0);
            mmap_dump_buffer->AppendPtr(cursor, 0);
            mmap_dump_buffer->AppendString(" - ", 0);
            mmap_dump_buffer->AppendPtr(last_address_of_unhooked + 1, 0);
            mmap_dump_buffer->AppendString("  unhooked ", 0);
            mmap_dump_buffer->AppendString(kMapsRegionTypeDict[type], 0);
            mmap_dump_buffer->AppendString(" ", 0);
            mmap_dump_buffer->AppendInt64(committed_size, 0);
            mmap_dump_buffer->AppendString("\n", 0);
          }
          cursor = last_address_of_unhooked + 1;
        }

        if (mmap_list_index < mmap_list_length &&
            mmap_list[mmap_list_index].first_address <= last_address &&
            mmap_dump_buffer) {
          bool trailing =
            mmap_list[mmap_list_index].first_address < first_address;
          bool continued =
            mmap_list[mmap_list_index].last_address > last_address;
          mmap_dump_buffer->AppendString(trailing ? " (" : "  ", 0);
          mmap_dump_buffer->AppendPtr(
              mmap_list[mmap_list_index].first_address, 0);
          mmap_dump_buffer->AppendString(trailing ? ")" : " ", 0);
          mmap_dump_buffer->AppendString("-", 0);
          mmap_dump_buffer->AppendString(continued ? "(" : " ", 0);
          mmap_dump_buffer->AppendPtr(
              mmap_list[mmap_list_index].last_address + 1, 0);
          mmap_dump_buffer->AppendString(continued ? ")" : " ", 0);
          mmap_dump_buffer->AppendString(" hooked ", 0);
          mmap_dump_buffer->AppendString(kMapsRegionTypeDict[type], 0);
          mmap_dump_buffer->AppendString("\n", 0);
        }
      } while (mmap_list_index < mmap_list_length &&
               mmap_list[mmap_list_index].last_address <= last_address);
    }
  }
}

void DeepHeapProfile::GlobalStats::SnapshotAllocations(
    DeepHeapProfile* deep_profile) {
  profiled_mmap_.Initialize();
  profiled_malloc_.Initialize();

  // malloc allocations.
  deep_profile->heap_profile_->alloc_address_map_->Iterate(RecordAlloc,
                                                           deep_profile);

  // mmap allocations.
  if (deep_profile->heap_profile_->mmap_address_map_) {
    deep_profile->heap_profile_->mmap_address_map_->Iterate(RecordMMap,
                                                            deep_profile);
    std::sort(deep_profile->mmap_list_,
              deep_profile->mmap_list_ + deep_profile->mmap_list_length_,
              ByFirstAddress);
  }
}

void DeepHeapProfile::GlobalStats::Unparse(TextBuffer* buffer) {
  RegionStats all_total;
  RegionStats unhooked_total;
  for (int i = 0; i < NUMBER_OF_MAPS_REGION_TYPES; ++i) {
    all_total.AddAnotherRegionStat(all_[i]);
    unhooked_total.AddAnotherRegionStat(unhooked_[i]);
  }

  // "# total (%lu) %c= profiled-mmap (%lu) + nonprofiled-* (%lu)\n"
  buffer->AppendString("# total (", 0);
  buffer->AppendUnsignedLong(all_total.committed_bytes(), 0);
  buffer->AppendString(") ", 0);
  buffer->AppendChar(all_total.committed_bytes() ==
                     profiled_mmap_.committed_bytes() +
                     unhooked_total.committed_bytes() ? '=' : '!');
  buffer->AppendString("= profiled-mmap (", 0);
  buffer->AppendUnsignedLong(profiled_mmap_.committed_bytes(), 0);
  buffer->AppendString(") + nonprofiled-* (", 0);
  buffer->AppendUnsignedLong(unhooked_total.committed_bytes(), 0);
  buffer->AppendString(")\n", 0);

  // "                               virtual    committed"
  buffer->AppendString("", 26);
  buffer->AppendString(kVirtualLabel, 12);
  buffer->AppendChar(' ');
  buffer->AppendString(kCommittedLabel, 12);
  buffer->AppendString("\n", 0);

  all_total.Unparse("total", buffer);
  all_[FILE_EXEC].Unparse("file-exec", buffer);
  all_[FILE_NONEXEC].Unparse("file-nonexec", buffer);
  all_[ANONYMOUS].Unparse("anonymous", buffer);
  all_[STACK].Unparse("stack", buffer);
  all_[OTHER].Unparse("other", buffer);
  unhooked_total.Unparse("nonprofiled-total", buffer);
  unhooked_[ABSENT].Unparse("nonprofiled-absent", buffer);
  unhooked_[ANONYMOUS].Unparse("nonprofiled-anonymous", buffer);
  unhooked_[FILE_EXEC].Unparse("nonprofiled-file-exec", buffer);
  unhooked_[FILE_NONEXEC].Unparse("nonprofiled-file-nonexec", buffer);
  unhooked_[STACK].Unparse("nonprofiled-stack", buffer);
  unhooked_[OTHER].Unparse("nonprofiled-other", buffer);
  profiled_mmap_.Unparse("profiled-mmap", buffer);
  profiled_malloc_.Unparse("profiled-malloc", buffer);
}

// static
bool DeepHeapProfile::GlobalStats::ByFirstAddress(const MMapListEntry& a,
                                                  const MMapListEntry& b) {
  return a.first_address < b.first_address;
}

// static
void DeepHeapProfile::GlobalStats::RecordAlloc(const void* pointer,
                                               AllocValue* alloc_value,
                                               DeepHeapProfile* deep_profile) {
  uint64 address = reinterpret_cast<uintptr_t>(pointer);
  size_t committed = deep_profile->memory_residence_info_getter_->CommittedSize(
      address, address + alloc_value->bytes - 1);

  DeepBucket* deep_bucket = deep_profile->deep_table_.Lookup(
      alloc_value->bucket(),
#if defined(TYPE_PROFILING)
      LookupType(pointer),
#endif
      /* is_mmap */ false);
  deep_bucket->committed_size += committed;
  deep_profile->stats_.profiled_malloc_.AddToVirtualBytes(alloc_value->bytes);
  deep_profile->stats_.profiled_malloc_.AddToCommittedBytes(committed);
}

// static
void DeepHeapProfile::GlobalStats::RecordMMap(const void* pointer,
                                              AllocValue* alloc_value,
                                              DeepHeapProfile* deep_profile) {
  uint64 address = reinterpret_cast<uintptr_t>(pointer);
  size_t committed = deep_profile->memory_residence_info_getter_->CommittedSize(
      address, address + alloc_value->bytes - 1);

  DeepBucket* deep_bucket = deep_profile->deep_table_.Lookup(
      alloc_value->bucket(),
#if defined(TYPE_PROFILING)
      NULL,
#endif
      /* is_mmap */ true);
  deep_bucket->committed_size += committed;
  deep_profile->stats_.profiled_mmap_.AddToVirtualBytes(alloc_value->bytes);
  deep_profile->stats_.profiled_mmap_.AddToCommittedBytes(committed);

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

// static
void DeepHeapProfile::WriteProcMaps(const char* prefix,
                                    int buffer_size,
                                    char raw_buffer[]) {
  char filename[100];
  snprintf(filename, sizeof(filename),
           "%s.%05d.maps", prefix, static_cast<int>(getpid()));

  RawFD fd = RawOpenForWriting(filename);
  RAW_DCHECK(fd != kIllegalRawFD, "");

  int length;
  bool wrote_all;
  length = tcmalloc::FillProcSelfMaps(raw_buffer, buffer_size, &wrote_all);
  RAW_DCHECK(wrote_all, "");
  RAW_DCHECK(length <= buffer_size, "");
  RawWrite(fd, raw_buffer, length);
  RawClose(fd);
}

// static
void DeepHeapProfile::CountMMap(const void* pointer,
                                AllocValue* alloc_value,
                                DeepHeapProfile* deep_profile) {
  ++deep_profile->num_mmap_allocations_;
}
#else  // DEEP_HEAP_PROFILE

DeepHeapProfile::DeepHeapProfile(HeapProfileTable* heap_profile,
                                 const char* prefix)
    : heap_profile_(heap_profile) {
}

DeepHeapProfile::~DeepHeapProfile() {
}

int DeepHeapProfile::FillOrderedProfile(char raw_buffer[], int buffer_size) {
  return heap_profile_->FillOrderedProfile(raw_buffer, buffer_size);
}

#endif  // DEEP_HEAP_PROFILE

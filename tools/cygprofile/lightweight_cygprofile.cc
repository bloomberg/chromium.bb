// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <unistd.h>

#include <atomic>
#include <cstdio>
#include <string>
#include <thread>
#include <vector>

#include "base/android/library_loader/anchor_functions.h"
#include "base/files/file.h"
#include "base/format_macros.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/strings/stringprintf.h"
#include "build/build_config.h"

#if !defined(ARCH_CPU_ARMEL)
#error Only supported on ARM.
#endif  // !defined(ARCH_CPU_ARMEL)

namespace {

// These are large overestimates, which is not an issue, as the data is
// allocated in .bss, and on linux doesn't take any actual memory when it's not
// touched.
constexpr size_t kBitfieldSize = 1 << 22;
constexpr size_t kMaxTextSizeInBytes = kBitfieldSize * (4 * 32);
constexpr size_t kMaxElements = 1 << 20;

// |RecordAddress()| adds an element to a concurrent bitset and to a concurrent
// append-only list of offsets.
//
// Ordering:
// Two consecutive calls to |RecordAddress()| from the same thread will be
// ordered in the same way in the result, as written by
// |StopAndDumpToFile()|. The result will contain exactly one instance of each
// unique offset relative to |kStartOfText| passed to |RecordAddress()|.
//
// Implementation:
// The "set" part is implemented with a bitfield, |g_offset|. The insertion
// order is recorded in |g_ordered_offsets|.
// This is not a class to make sure there isn't a static constructor, as it
// would cause issue with an instrumented static constructor calling this code.
//
// Limitations:
// - Only records offsets to addresses between |kStartOfText| and |kEndOfText|.
// - Capacity of the set is limited by |kMaxElements|.
// - Some insertions at the end of collection may be lost.

// |g_offsets| and |g_ordered_offsets| are allocated in .bss, as
// std::atomic<uint32_t> and std::atomic<size_t> are guarangeed to behave like
// their respective template type parameters with respect to size and
// initialization.
// Having these arrays in .bss simplifies initialization.
std::atomic<uint32_t> g_offsets[kBitfieldSize];
// Non-null iff collection is enabled. Also use to as the pointer to the offset
// array to save a global load in the instrumentation.
std::atomic<std::atomic<uint32_t>*> g_enabled_and_offsets = {g_offsets};

// Ordered list of recorded offsets.
std::atomic<size_t> g_ordered_offsets[kMaxElements];
// Next free slot.
std::atomic<size_t> g_ordered_offset_index;

// Stops recording.
void Disable() {
  g_enabled_and_offsets.store(nullptr, std::memory_order_relaxed);
}

// Records that |address| has been reached, if recording is enabled.
// To avoid any risk of infinite recursion, this *must* *never* call any
// instrumented function.
void RecordAddress(size_t address) {
  auto* offsets = g_enabled_and_offsets.load(std::memory_order_relaxed);
  if (!offsets)
    return;

  if (UNLIKELY(address < base::android::kStartOfText ||
               address > base::android::kEndOfText)) {
    Disable();
    LOG(FATAL) << "Return address out of bounds (are dummy functions in the "
               << "orderfile?)";
  }

  size_t offset = address - base::android::kStartOfText;
  static_assert(sizeof(int) == 4,
                "Collection and processing code assumes that sizeof(int) == 4");
  size_t index = offset / 4;

  // Atomically set the corresponding bit in the array.
  std::atomic<uint32_t>* element = offsets + (index / 32);
  // First, a racy check. This saves a CAS if the bit is already set, and
  // allows the cache line to remain shared acoss CPUs in this case.
  uint32_t value = element->load(std::memory_order_relaxed);
  uint32_t mask = 1 << (index % 32);
  if (value & mask)
    return;

  auto before = element->fetch_or(mask, std::memory_order_relaxed);
  if (before & mask)
    return;

  // We were the first one to set the element, record it in the ordered
  // elements list.
  // Use relaxed ordering, as the value is not published, or used for
  // synchronization.
  size_t ordered_offsets_index =
      g_ordered_offset_index.fetch_add(1, std::memory_order_relaxed);
  if (UNLIKELY(ordered_offsets_index >= kMaxElements)) {
    Disable();
    LOG(FATAL) << "Too many reached offsets";
  }
  g_ordered_offsets[ordered_offsets_index].store(offset,
                                                 std::memory_order_relaxed);
}

// Stops recording, and outputs the data to |path|.
void StopAndDumpToFile(const base::FilePath& path) {
  Disable();
  auto file = base::File(base::FilePath(path), base::File::FLAG_CREATE_ALWAYS |
                                                   base::File::FLAG_WRITE);
  if (!file.IsValid()) {
    PLOG(ERROR) << "Could not open " << path;
    return;
  }

  std::atomic_thread_fence(std::memory_order_seq_cst);
  // |g_ordered_offset_index| is the index of the next insertion.
  size_t count = g_ordered_offset_index.load(std::memory_order_relaxed) - 1;
  for (size_t i = 0; i < count; i++) {
    // |g_ordered_offsets| is initialized to 0, so a 0 in the middle of it
    // indicates a case where the index was incremented, but the write is not
    // visible in this thread yet. Safe to skip, also because the function at
    // the start of text is never called.
    auto offset = g_ordered_offsets[i].load(std::memory_order_relaxed);
    if (!offset)
      continue;
    auto offset_str = base::StringPrintf("%" PRIuS "\n", offset);
    file.WriteAtCurrentPos(offset_str.c_str(),
                           static_cast<int>(offset_str.size()));
  }
}

// Disables the logging and dumps the result after |kDelayInSeconds|.
class DelayedDumper {
 public:
  DelayedDumper() {
    CHECK_LT(base::android::kEndOfText - base::android::kStartOfText,
             kMaxTextSizeInBytes);
    base::android::CheckOrderingSanity();

    std::thread([]() {
      sleep(kDelayInSeconds);
      auto path = base::StringPrintf(
          "/data/local/tmp/chrome/cyglog/"
          "cygprofile-instrumented-code-hitmap-%d.txt",
          getpid());
      StopAndDumpToFile(base::FilePath(path));
    })
        .detach();
  }

  static constexpr int kDelayInSeconds = 30;
};

// Static initializer on purpose. Will disable instrumentation after
// |kDelayInSeconds|.
DelayedDumper g_dump_later;

}  // namespace

extern "C" {

// Since this function relies on the return address, if it's not inlined and
// __cyg_profile_func_enter() is called below, then the return address will
// be inside __cyg_profile_func_enter(). To prevent that, force inlining.
// We cannot use ALWAYS_INLINE from src/base/compiler_specific.h, as it doesn't
// always map to always_inline, for instance when NDEBUG is not defined.
__attribute__((__always_inline__)) void mcount() {
  RecordAddress(reinterpret_cast<size_t>(__builtin_return_address(0)));
}

void __cyg_profile_func_enter(void* unused1, void* unused2) {
  // Requires __always_inline__ on mcount(), see above.
  mcount();
}

void __cyg_profile_func_exit(void* unused1, void* unused2) {}

}  // extern "C"

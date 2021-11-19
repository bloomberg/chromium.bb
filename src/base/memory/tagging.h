// Copyright (c) 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_MEMORY_TAGGING_H_
#define BASE_MEMORY_TAGGING_H_

// This file contains method definitions to support Armv8.5-A's memory tagging
// extension.
#include <stddef.h>
#include <stdint.h>
#include "base/base_export.h"
#include "base/compiler_specific.h"
#include "build/build_config.h"

#if defined(ARCH_CPU_ARM64) && defined(__clang__) && \
    (defined(OS_LINUX) || defined(OS_ANDROID))
#define HAS_MEMORY_TAGGING 1
#endif

constexpr int kMemTagGranuleSize = 16u;
#if defined(HAS_MEMORY_TAGGING)
constexpr uint64_t kMemTagUnmask = 0x00ffffffffffffffuLL;
#else
constexpr uint64_t kMemTagUnmask = 0xffffffffffffffffuLL;
#endif

namespace base {
namespace memory {

// Enum configures Arm's MTE extension to operate in different modes
enum class TagViolationReportingMode {
  // Default settings
  kUndefined,
  // MTE explicitly disabled.
  kDisabled,
  // Precise tag violation reports, higher overhead. Good for unittests
  // and security critical threads.
  kSynchronous,
  // Imprecise tag violation reports (async mode). Lower overhead.
  kAsynchronous,
};

#if defined(OS_ANDROID)
// Changes the memory tagging mode for all threads in the current process.
BASE_EXPORT void ChangeMemoryTaggingModeForAllThreadsPerProcess(
    TagViolationReportingMode);
#endif

// Changes the memory tagging mode for the calling thread.
BASE_EXPORT void ChangeMemoryTaggingModeForCurrentThread(
    TagViolationReportingMode);

// Gets the memory tagging mode for the calling thread.
BASE_EXPORT TagViolationReportingMode GetMemoryTaggingModeForCurrentThread();

// Called by the partition allocator after initial startup, this detects MTE
// support in the current CPU and replaces the active tagging intrinsics with
// MTE versions if needed.
BASE_EXPORT void InitializeMTESupportIfNeeded();

// These global function pointers hold the implementations of the tagging
// intrinsics (TagMemoryRangeRandomly, TagMemoryRangeIncrement, RemaskPtr).
// They are designed to be callable without taking a branch. They are initially
// set to no-op functions in tagging.cc, but can be replaced with MTE-capable
// ones through InitializeMTEIfNeeded(). This is conceptually similar to an
// ifunc (but less secure) - we do it this way for now because the CrazyLinker
// (needed for supporting old Android versions) doesn't support them. Initial
// solution should be good enough for fuzzing/debug, ideally needs fixing for
// async deployment on end-user devices.
namespace internal {
using RemaskPtrInternalFn = void*(void* ptr);
using TagMemoryRangeIncrementInternalFn = void*(void* ptr, size_t size);

using TagMemoryRangeRandomlyInternalFn = void*(void* ptr,
                                               size_t size,
                                               uint64_t mask);
extern BASE_EXPORT TagMemoryRangeRandomlyInternalFn*
    global_tag_memory_range_randomly_fn;
extern BASE_EXPORT TagMemoryRangeIncrementInternalFn*
    global_tag_memory_range_increment_fn;
extern BASE_EXPORT RemaskPtrInternalFn* global_remask_void_ptr_fn;
}  // namespace internal

// Increments the tag of the memory range ptr. Useful for provable revocations
// (e.g. free). Returns the pointer with the new tag. Ensures that the entire
// range is set to the same tag.
template <typename T>
ALWAYS_INLINE T* TagMemoryRangeIncrement(T* ptr, size_t size) {
#if defined(HAS_MEMORY_TAGGING)
  return reinterpret_cast<T*>(
      internal::global_tag_memory_range_increment_fn(ptr, size));
#else
  return ptr;
#endif
}

// Randomly changes the tag of the ptr memory range. Useful for initial random
// initialization. Returns the pointer with the new tag. Ensures that the entire
// range is set to the same tag.
template <typename T>
ALWAYS_INLINE T* TagMemoryRangeRandomly(T* ptr,
                                        size_t size,
                                        uint64_t mask = 0u) {
#if defined(HAS_MEMORY_TAGGING)
  return reinterpret_cast<T*>(
      internal::global_tag_memory_range_randomly_fn(ptr, size, mask));
#else
  return ptr;
#endif
}

// Gets a version of ptr that's safe to dereference.
template <typename T>
ALWAYS_INLINE T* RemaskPtr(T* ptr) {
#if defined(HAS_MEMORY_TAGGING)
  return reinterpret_cast<T*>(internal::global_remask_void_ptr_fn(ptr));
#else
  return ptr;
#endif
}

ALWAYS_INLINE uintptr_t UnmaskPtr(uintptr_t ptr) {
#if defined(HAS_MEMORY_TAGGING)
  return ptr & kMemTagUnmask;
#else
  return ptr;
#endif
}

// Strips the tag bits off ptr.
template <typename T>
ALWAYS_INLINE T* UnmaskPtr(T* ptr) {
  return reinterpret_cast<T*>(UnmaskPtr(reinterpret_cast<uintptr_t>(ptr)));
}

}  // namespace memory
}  // namespace base

#endif  // BASE_MEMORY_TAGGING_H_

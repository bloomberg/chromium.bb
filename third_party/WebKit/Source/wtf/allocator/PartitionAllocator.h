// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WTF_PartitionAllocator_h
#define WTF_PartitionAllocator_h

// This is the allocator that is used for allocations that are not on the
// traced, garbage collected heap. It uses FastMalloc for collections,
// but uses the partition allocator for the backing store of the collections.

#include "base/allocator/partition_allocator/partition_alloc.h"
#include "third_party/WebKit/Source/wtf/Allocator.h"
#include "wtf/Assertions.h"
#include "wtf/TypeTraits.h"
#include "wtf/WTFExport.h"
#include <string.h>

namespace WTF {

class PartitionAllocatorDummyVisitor {
  DISALLOW_NEW();
};

class WTF_EXPORT PartitionAllocator {
 public:
  typedef PartitionAllocatorDummyVisitor Visitor;
  static const bool isGarbageCollected = false;

  template<typename T>
  static size_t maxElementCountInBackingStore() {
    return base::kGenericMaxDirectMapped / sizeof(T);
  }

  template <typename T>
  static size_t quantizedSize(size_t count) {
    CHECK_LE(count, maxElementCountInBackingStore<T>());
    return PartitionAllocActualSize(WTF::Partitions::bufferPartition(),
                                    count * sizeof(T));
  }
  template <typename T>
  static T* allocateVectorBacking(size_t size) {
    return reinterpret_cast<T*>(
        allocateBacking(size, WTF_HEAP_PROFILER_TYPE_NAME(T)));
  }
  template <typename T>
  static T* allocateExpandedVectorBacking(size_t size) {
    return reinterpret_cast<T*>(
        allocateBacking(size, WTF_HEAP_PROFILER_TYPE_NAME(T)));
  }
  static void freeVectorBacking(void* address);
  static inline bool expandVectorBacking(void*, size_t) { return false; }
  static inline bool shrinkVectorBacking(void* address,
                                         size_t quantizedCurrentSize,
                                         size_t quantizedShrunkSize) {
    // Optimization: if we're downsizing inside the same allocator bucket,
    // we can skip reallocation.
    return quantizedCurrentSize == quantizedShrunkSize;
  }
  template <typename T>
  static T* allocateInlineVectorBacking(size_t size) {
    return allocateVectorBacking<T>(size);
  }
  static inline void freeInlineVectorBacking(void* address) {
    freeVectorBacking(address);
  }
  static inline bool expandInlineVectorBacking(void*, size_t) { return false; }
  static inline bool shrinkInlineVectorBacking(void* address,
                                               size_t quantizedCurrentSize,
                                               size_t quantizedShrunkSize) {
    return shrinkVectorBacking(address, quantizedCurrentSize,
                               quantizedShrunkSize);
  }

  template <typename T, typename HashTable>
  static T* allocateHashTableBacking(size_t size) {
    return reinterpret_cast<T*>(
        allocateBacking(size, WTF_HEAP_PROFILER_TYPE_NAME(T)));
  }
  template <typename T, typename HashTable>
  static T* allocateZeroedHashTableBacking(size_t size) {
    void* result = allocateBacking(size, WTF_HEAP_PROFILER_TYPE_NAME(T));
    memset(result, 0, size);
    return reinterpret_cast<T*>(result);
  }
  static void freeHashTableBacking(void* address);

  template <typename Return, typename Metadata>
  static Return malloc(size_t size, const char* typeName) {
    return reinterpret_cast<Return>(
        WTF::Partitions::fastMalloc(size, typeName));
  }

  static inline bool expandHashTableBacking(void*, size_t) { return false; }
  static void free(void* address) { WTF::Partitions::fastFree(address); }
  template <typename T>
  static void* newArray(size_t bytes) {
    return malloc<void*, void>(bytes, WTF_HEAP_PROFILER_TYPE_NAME(T));
  }
  static void deleteArray(void* ptr) {
    free(ptr);  // Not the system free, the one from this class.
  }

  static bool isAllocationAllowed() { return true; }

  static void enterGCForbiddenScope() {}
  static void leaveGCForbiddenScope() {}

 private:
  static void* allocateBacking(size_t, const char* typeName);
};

// Specializations for heap profiling, so type profiling of |char| is possible
// even in official builds (because |char| makes up a large portion of the
// heap.)
template <>
WTF_EXPORT char* PartitionAllocator::allocateVectorBacking<char>(size_t);
template <>
WTF_EXPORT char* PartitionAllocator::allocateExpandedVectorBacking<char>(
    size_t);

}  // namespace WTF

#define USE_ALLOCATOR(ClassName, Allocator)                       \
 public:                                                          \
  void* operator new(size_t size) {                               \
    return Allocator::template malloc<void*, ClassName>(          \
        size, WTF_HEAP_PROFILER_TYPE_NAME(ClassName));            \
  }                                                               \
  void operator delete(void* p) { Allocator::free(p); }           \
  void* operator new[](size_t size) {                             \
    return Allocator::template newArray<ClassName>(size);         \
  }                                                               \
  void operator delete[](void* p) { Allocator::deleteArray(p); }  \
  void* operator new(size_t, NotNullTag, void* location) {        \
    DCHECK(location);                                             \
    return location;                                              \
  }                                                               \
  void* operator new(size_t, void* location) { return location; } \
                                                                  \
 private:                                                         \
  typedef int __thisIsHereToForceASemicolonAfterThisMacro

#endif  // WTF_PartitionAllocator_h

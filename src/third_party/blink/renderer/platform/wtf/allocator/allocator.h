// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_WTF_ALLOCATOR_ALLOCATOR_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_WTF_ALLOCATOR_ALLOCATOR_H_

#include <atomic>

#include "base/allocator/buildflags.h"
#include "base/check_op.h"
#include "build/build_config.h"
#include "third_party/blink/renderer/platform/wtf/allocator/partitions.h"
#include "third_party/blink/renderer/platform/wtf/type_traits.h"

#if BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
#if !BUILDFLAG(IS_APPLE)
// FastMalloc() defers to malloc() in this case, and including its header
// ensures that the compiler knows that malloc() is "special", e.g. that it
// returns properly-aligned, distinct memory locations.
#include <malloc.h>
#elif BUILDFLAG(IS_APPLE)
// malloc.h doesn't exist on Apple OSes (it's in malloc/malloc.h), but the
// definitions we want are actually in stdlib.h.
#include <stdlib.h>
#endif  // BUILDFLAG(IS_APPLE)
#else
#include "base/allocator/partition_allocator/partition_alloc.h"
#endif  // BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)

namespace WTF {

namespace internal {
// A dummy class used in following macros.
class __thisIsHereToForceASemicolonAfterThisMacro;
}  // namespace internal

// Classes that contain references to garbage-collected objects but aren't
// themselves garbaged allocated, have some extra macros available which
// allows their use to be restricted to cases where the garbage collector
// is able to discover their references. These macros will be useful for
// non-garbage-collected objects to avoid unintended allocations.
//
// STACK_ALLOCATED(): Use if the object is only stack allocated.
// Garbage-collected objects should be in raw pointers.
//
// DISALLOW_NEW(): Cannot be allocated with new operators but can be a
// part of object, a value object in collections or stack allocated. If it has
// Members you need a trace method and the containing object needs to call that
// trace method.
//
#define DISALLOW_NEW()                                                        \
 public:                                                                      \
  using IsDisallowNewMarker = int;                                            \
  void* operator new(size_t, NotNullTag, void* location) { return location; } \
  void* operator new(size_t, void* location) { return location; }             \
                                                                              \
 private:                                                                     \
  void* operator new(size_t) = delete;                                        \
                                                                              \
 public:                                                                      \
  friend class ::WTF::internal::__thisIsHereToForceASemicolonAfterThisMacro

#define STATIC_ONLY(Type)                                 \
  Type() = delete;                                        \
  Type(const Type&) = delete;                             \
  Type& operator=(const Type&) = delete;                  \
  void* operator new(size_t) = delete;                    \
  void* operator new(size_t, NotNullTag, void*) = delete; \
  void* operator new(size_t, void*) = delete

#define STACK_ALLOCATED()                                  \
 public:                                                   \
  using IsStackAllocatedTypeMarker [[maybe_unused]] = int; \
                                                           \
 private:                                                  \
  void* operator new(size_t) = delete;                     \
  void* operator new(size_t, NotNullTag, void*) = delete;  \
  void* operator new(size_t, void*) = delete

// Provides customizable overrides of fastMalloc/fastFree and operator
// new/delete
//
// Provided functionality:
//    Macro: USING_FAST_MALLOC
//
// Example usage:
//    class Widget {
//        USING_FAST_MALLOC(Widget)
//    ...
//    };
//
//    struct Data {
//        USING_FAST_MALLOC(Data)
//    public:
//    ...
//    };
//

// In official builds, do not include type info string literals to avoid
// bloating the binary.
#if defined(OFFICIAL_BUILD)
#define WTF_HEAP_PROFILER_TYPE_NAME(T) nullptr
#else
#define WTF_HEAP_PROFILER_TYPE_NAME(T) ::WTF::GetStringWithTypeName<T>()
#endif

// Both of these macros enable fast malloc and provide type info to the heap
// profiler. The regular macro does not provide type info in official builds,
// to avoid bloating the binary with type name strings. The |WITH_TYPE_NAME|
// variant provides type info unconditionally, so it should be used sparingly.
// Furthermore, the |WITH_TYPE_NAME| variant does not work if |type| is a
// template argument; |USING_FAST_MALLOC| does.
#define USING_FAST_MALLOC(type) \
  USING_FAST_MALLOC_INTERNAL(type, WTF_HEAP_PROFILER_TYPE_NAME(type))
#define USING_FAST_MALLOC_WITH_TYPE_NAME(type) \
  USING_FAST_MALLOC_INTERNAL(type, #type)

#define USING_FAST_MALLOC_INTERNAL(type, typeName)                    \
 public:                                                              \
  void* operator new(size_t, void* p) { return p; }                   \
  void* operator new[](size_t, void* p) { return p; }                 \
                                                                      \
  void* operator new(size_t size) {                                   \
    return ::WTF::Partitions::FastMalloc(size, typeName);             \
  }                                                                   \
                                                                      \
  void operator delete(void* p) { ::WTF::Partitions::FastFree(p); }   \
                                                                      \
  void* operator new[](size_t size) {                                 \
    return ::WTF::Partitions::FastMalloc(size, typeName);             \
  }                                                                   \
                                                                      \
  void operator delete[](void* p) { ::WTF::Partitions::FastFree(p); } \
  void* operator new(size_t, NotNullTag, void* location) {            \
    DCHECK(location);                                                 \
    return location;                                                  \
  }                                                                   \
                                                                      \
 private:                                                             \
  friend class ::WTF::internal::__thisIsHereToForceASemicolonAfterThisMacro

}  // namespace WTF

// This version of placement new omits a 0 check.
enum class NotNullTag { kNotNull };
inline void* operator new(size_t, NotNullTag, void* location) {
  DCHECK(location);
  return location;
}

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_WTF_ALLOCATOR_ALLOCATOR_H_

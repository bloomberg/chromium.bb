// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_MEMORY_RAW_PTR_H_
#define BASE_MEMORY_RAW_PTR_H_

#include <stddef.h>
#include <stdint.h>

#include <cstddef>
#include <functional>
#include <type_traits>
#include <utility>

#include "base/allocator/buildflags.h"
#include "base/allocator/partition_allocator/partition_alloc_config.h"
#include "base/check.h"
#include "base/compiler_specific.h"
#include "base/dcheck_is_on.h"
#include "base/trace_event/base_tracing_forward.h"
#include "build/build_config.h"
#include "build/buildflag.h"

#if BUILDFLAG(USE_BACKUP_REF_PTR) || \
    defined(PA_USE_MTE_CHECKED_PTR_WITH_64_BITS_POINTERS)
// USE_BACKUP_REF_PTR implies USE_PARTITION_ALLOC, needed for code under
// allocator/partition_allocator/ to be built.
#include "base/allocator/partition_allocator/address_pool_manager_bitmap.h"
#include "base/allocator/partition_allocator/partition_address_space.h"
#include "base/allocator/partition_allocator/partition_alloc_constants.h"
#include "base/base_export.h"
#endif  // BUILDFLAG(USE_BACKUP_REF_PTR) ||
        // defined(PA_USE_MTE_CHECKED_PTR_WITH_64_BITS_POINTERS)

#if defined(PA_USE_MTE_CHECKED_PTR_WITH_64_BITS_POINTERS)
#include "base/allocator/partition_allocator/partition_tag.h"
#include "base/allocator/partition_allocator/tagging.h"
#endif  // defined(PA_USE_MTE_CHECKED_PTR_WITH_64_BITS_POINTERS)

#if BUILDFLAG(IS_WIN)
#include "base/win/windows_types.h"
#endif

// Marks a field as excluded from the raw_ptr usage enforcement clang plugin.
// Example: RAW_PTR_EXCLUSION Foo* foo_;
#define RAW_PTR_EXCLUSION __attribute__((annotate("raw_ptr_exclusion")))

namespace cc {
class Scheduler;
}
namespace base::internal {
class DelayTimerBase;
}
namespace content::responsiveness {
class Calculator;
}

namespace base {

// NOTE: All methods should be `ALWAYS_INLINE`. raw_ptr is meant to be a
// lightweight replacement of a raw pointer, hence performance is critical.

namespace internal {
// These classes/structures are part of the raw_ptr implementation.
// DO NOT USE THESE CLASSES DIRECTLY YOURSELF.

struct RawPtrNoOpImpl {
  // Wraps a pointer.
  template <typename T>
  static ALWAYS_INLINE T* WrapRawPtr(T* ptr) {
    return ptr;
  }

  // Notifies the allocator when a wrapped pointer is being removed or replaced.
  template <typename T>
  static ALWAYS_INLINE void ReleaseWrappedPtr(T*) {}

  // Unwraps the pointer, while asserting that memory hasn't been freed. The
  // function is allowed to crash on nullptr.
  template <typename T>
  static ALWAYS_INLINE T* SafelyUnwrapPtrForDereference(T* wrapped_ptr) {
    return wrapped_ptr;
  }

  // Unwraps the pointer, while asserting that memory hasn't been freed. The
  // function must handle nullptr gracefully.
  template <typename T>
  static ALWAYS_INLINE T* SafelyUnwrapPtrForExtraction(T* wrapped_ptr) {
    return wrapped_ptr;
  }

  // Unwraps the pointer, without making an assertion on whether memory was
  // freed or not.
  template <typename T>
  static ALWAYS_INLINE T* UnsafelyUnwrapPtrForComparison(T* wrapped_ptr) {
    return wrapped_ptr;
  }

  // Upcasts the wrapped pointer.
  template <typename To, typename From>
  static ALWAYS_INLINE constexpr To* Upcast(From* wrapped_ptr) {
    static_assert(std::is_convertible<From*, To*>::value,
                  "From must be convertible to To.");
    // Note, this cast may change the address if upcasting to base that lies in
    // the middle of the derived object.
    return wrapped_ptr;
  }

  // Advance the wrapped pointer by |delta| bytes.
  template <typename T>
  static ALWAYS_INLINE T* Advance(T* wrapped_ptr, ptrdiff_t delta_elems) {
    return wrapped_ptr + delta_elems;
  }

  // Returns a copy of a wrapped pointer, without making an assertion on whether
  // memory was freed or not.
  template <typename T>
  static ALWAYS_INLINE T* Duplicate(T* wrapped_ptr) {
    return wrapped_ptr;
  }

  // This is for accounting only, used by unit tests.
  static ALWAYS_INLINE void IncrementSwapCountForTest() {}
  static ALWAYS_INLINE void IncrementPointerToMemberOperatorCountForTest() {}
};

#if defined(PA_USE_MTE_CHECKED_PTR_WITH_64_BITS_POINTERS)

constexpr int kValidAddressBits = 48;
constexpr uintptr_t kAddressMask = (1ull << kValidAddressBits) - 1;
constexpr int kTagBits = sizeof(uintptr_t) * 8 - kValidAddressBits;

// MTECheckedPtr has no business with the topmost bits reserved for the
// tag used by true ARM MTE, so we strip it out here.
constexpr uintptr_t kTagMask =
    ~kAddressMask & partition_alloc::internal::kMemTagUnmask;

constexpr int kTopBitShift = 63;
constexpr uintptr_t kTopBit = 1ull << kTopBitShift;
static_assert(kTopBit << 1 == 0, "kTopBit should really be the top bit");
static_assert((kTopBit & kTagMask) > 0,
              "kTopBit bit must be inside the tag region");

// This functionality is outside of MTECheckedPtrImpl, so that it can be
// overridden by tests.
struct MTECheckedPtrImplPartitionAllocSupport {
  // Checks if the necessary support is enabled in PartitionAlloc for `ptr`.
  template <typename T>
  static ALWAYS_INLINE bool EnabledForPtr(T* ptr) {
    auto as_uintptr =
        partition_alloc::internal::UnmaskPtr(reinterpret_cast<uintptr_t>(ptr));
    // MTECheckedPtr algorithms work only when memory is
    // allocated by PartitionAlloc, from normal buckets pool.
    //
    // TODO(crbug.com/1307514): Allow direct-map buckets.
    return IsManagedByPartitionAlloc(as_uintptr) &&
           IsManagedByNormalBuckets(as_uintptr);
  }

  // Returns pointer to the tag that protects are pointed by |ptr|.
  static ALWAYS_INLINE void* TagPointer(uintptr_t ptr) {
    return partition_alloc::PartitionTagPointer(ptr);
  }
};

template <typename PartitionAllocSupport>
struct MTECheckedPtrImpl {
  // This implementation assumes that pointers are 64 bits long and at least 16
  // top bits are unused. The latter is harder to verify statically, but this is
  // true for all currently supported 64-bit architectures (DCHECK when wrapping
  // will verify that).
  static_assert(sizeof(void*) >= 8, "Need 64-bit pointers");

  // Wraps a pointer, and returns its uintptr_t representation.
  template <typename T>
  static ALWAYS_INLINE T* WrapRawPtr(T* ptr) {
    uintptr_t addr = reinterpret_cast<uintptr_t>(ptr);
    DCHECK_EQ(ExtractTag(addr), 0ull);

    // Return a not-wrapped |addr|, if it's either nullptr or if the protection
    // for this pointer is disabled.
    if (!PartitionAllocSupport::EnabledForPtr(ptr)) {
      return reinterpret_cast<T*>(addr);
    }

    // Read the tag and place it in the top bits of the address.
    // Even if PartitionAlloc's tag has less than kTagBits, we'll read
    // what's given and pad the rest with 0s.
    static_assert(sizeof(partition_alloc::PartitionTag) * 8 <= kTagBits, "");
    uintptr_t tag = *(static_cast<volatile partition_alloc::PartitionTag*>(
        PartitionAllocSupport::TagPointer(addr)));

    tag <<= kValidAddressBits;
    addr |= tag;
    return reinterpret_cast<T*>(addr);
  }

  // Notifies the allocator when a wrapped pointer is being removed or replaced.
  // No-op for MTECheckedPtrImpl.
  template <typename T>
  static ALWAYS_INLINE void ReleaseWrappedPtr(T*) {}

  // Unwraps the pointer's uintptr_t representation, while asserting that memory
  // hasn't been freed. The function is allowed to crash on nullptr.
  template <typename T>
  static ALWAYS_INLINE T* SafelyUnwrapPtrForDereference(T* wrapped_ptr) {
    uintptr_t wrapped_addr = reinterpret_cast<uintptr_t>(wrapped_ptr);
    uintptr_t tag = ExtractTag(wrapped_addr);
    if (tag > 0) {
      // Read the tag provided by PartitionAlloc.
      //
      // Cast to volatile to ensure memory is read. E.g. in a tight loop, the
      // compiler could cache the value in a register and thus could miss that
      // another thread freed memory and changed tag.
      uintptr_t read_tag =
          *static_cast<volatile partition_alloc::PartitionTag*>(
              PartitionAllocSupport::TagPointer(ExtractAddress(wrapped_addr)));
      if (UNLIKELY(tag != read_tag))
        IMMEDIATE_CRASH();
      return reinterpret_cast<T*>(ExtractAddress(wrapped_addr));
    }
    return wrapped_ptr;
  }

  // Unwraps the pointer's uintptr_t representation, while asserting that memory
  // hasn't been freed. The function must handle nullptr gracefully.
  template <typename T>
  static ALWAYS_INLINE T* SafelyUnwrapPtrForExtraction(T* wrapped_ptr) {
    // SafelyUnwrapPtrForDereference handles nullptr case well.
    return SafelyUnwrapPtrForDereference(wrapped_ptr);
  }

  // Unwraps the pointer's uintptr_t representation, without making an assertion
  // on whether memory was freed or not.
  template <typename T>
  static ALWAYS_INLINE T* UnsafelyUnwrapPtrForComparison(T* wrapped_ptr) {
    return ExtractPtr(wrapped_ptr);
  }

  // Upcasts the wrapped pointer.
  template <typename To, typename From>
  static ALWAYS_INLINE constexpr To* Upcast(From* wrapped_ptr) {
    static_assert(std::is_convertible<From*, To*>::value,
                  "From must be convertible to To.");

    // The top-bit tag must not affect the result of upcast.
    return static_cast<To*>(wrapped_ptr);
  }

  // Advance the wrapped pointer by |delta| bytes.
  template <typename T>
  static ALWAYS_INLINE T* Advance(T* wrapped_ptr, ptrdiff_t delta_elem) {
    return wrapped_ptr + delta_elem;
  }

  // Returns a copy of a wrapped pointer, without making an assertion
  // on whether memory was freed or not.
  template <typename T>
  static ALWAYS_INLINE T* Duplicate(T* wrapped_ptr) {
    return wrapped_ptr;
  }

  // This is for accounting only, used by unit tests.
  static ALWAYS_INLINE void IncrementSwapCountForTest() {}
  static ALWAYS_INLINE void IncrementPointerToMemberOperatorCountForTest() {}

 private:
  static ALWAYS_INLINE uintptr_t ExtractAddress(uintptr_t wrapped_ptr) {
    return wrapped_ptr & kAddressMask;
  }

  template <typename T>
  static ALWAYS_INLINE T* ExtractPtr(T* wrapped_ptr) {
    return reinterpret_cast<T*>(
        ExtractAddress(reinterpret_cast<uintptr_t>(wrapped_ptr)));
  }

  static ALWAYS_INLINE uintptr_t ExtractTag(uintptr_t wrapped_ptr) {
    return (wrapped_ptr & kTagMask) >> kValidAddressBits;
  }
};

#endif  // defined(PA_USE_MTE_CHECKED_PTR_WITH_64_BITS_POINTERS)

#if BUILDFLAG(USE_BACKUP_REF_PTR)

#if DCHECK_IS_ON() || BUILDFLAG(ENABLE_BACKUP_REF_PTR_SLOW_CHECKS)
BASE_EXPORT void CheckThatAddressIsntWithinFirstPartitionPage(
    uintptr_t address);
#endif

template <bool AllowDangling = false>
struct BackupRefPtrImpl {
  // Note that `BackupRefPtrImpl` itself is not thread-safe. If multiple threads
  // modify the same smart pointer object without synchronization, a data race
  // will occur.

  static ALWAYS_INLINE bool IsSupportedAndNotNull(uintptr_t address) {
    // This covers the nullptr case, as address 0 is never in GigaCage.
    bool is_in_brp_pool = IsManagedByPartitionAllocBRPPool(address);

    // There are many situations where the compiler can prove that
    // ReleaseWrappedPtr is called on a value that is always nullptr, but the
    // way the check above is written, the compiler can't prove that nullptr is
    // not managed by PartitionAlloc; and so the compiler has to emit a useless
    // check and dead code.
    // To avoid that without making the runtime check slower, explicitly promise
    // to the compiler that is_in_brp_pool will always be false for nullptr.
    //
    // This condition would look nicer and might also theoretically be nicer for
    // the optimizer if it was written as "if (!address) { ... }", but
    // LLVM currently has issues with optimizing that away properly; see:
    // https://bugs.llvm.org/show_bug.cgi?id=49403
    // https://reviews.llvm.org/D97848
    // https://chromium-review.googlesource.com/c/chromium/src/+/2727400/2/base/memory/checked_ptr.h#120
#if DCHECK_IS_ON() || BUILDFLAG(ENABLE_BACKUP_REF_PTR_SLOW_CHECKS)
    CHECK(address || !is_in_brp_pool);
#endif
#if HAS_BUILTIN(__builtin_assume)
    __builtin_assume(address || !is_in_brp_pool);
#endif

    // There may be pointers immediately after the allocation, e.g.
    //   {
    //     // Assume this allocation happens outside of PartitionAlloc.
    //     raw_ptr<T> ptr = new T[20];
    //     for (size_t i = 0; i < 20; i ++) { ptr++; }
    //   }
    //
    // Such pointers are *not* at risk of accidentally falling into BRP pool,
    // because:
    // 1) On 64-bit systems, BRP pool is preceded by a forbidden region.
    // 2) On 32-bit systems, the guard pages and metadata of super pages in BRP
    //    pool aren't considered to be part of that pool.
    //
    // This allows us to make a stronger assertion that if
    // IsManagedByPartitionAllocBRPPool returns true for a valid pointer,
    // it must be at least partition page away from the beginning of a super
    // page.
#if DCHECK_IS_ON() || BUILDFLAG(ENABLE_BACKUP_REF_PTR_SLOW_CHECKS)
    if (is_in_brp_pool) {
      CheckThatAddressIsntWithinFirstPartitionPage(address);
    }
#endif

    return is_in_brp_pool;
  }

  // Wraps a pointer.
  template <typename T>
  static ALWAYS_INLINE T* WrapRawPtr(T* ptr) {
    uintptr_t address = reinterpret_cast<uintptr_t>(ptr);
    if (IsSupportedAndNotNull(address)) {
#if DCHECK_IS_ON() || BUILDFLAG(ENABLE_BACKUP_REF_PTR_SLOW_CHECKS)
      CHECK(ptr != nullptr);
#endif
      AcquireInternal(address);
    }
#if !defined(PA_HAS_64_BITS_POINTERS)
    else {
      AddressPoolManagerBitmap::BanSuperPageFromBRPPool(address);
    }
#endif

    return ptr;
  }

  // Notifies the allocator when a wrapped pointer is being removed or replaced.
  template <typename T>
  static ALWAYS_INLINE void ReleaseWrappedPtr(T* wrapped_ptr) {
    uintptr_t address = reinterpret_cast<uintptr_t>(wrapped_ptr);
    if (IsSupportedAndNotNull(address)) {
#if DCHECK_IS_ON() || BUILDFLAG(ENABLE_BACKUP_REF_PTR_SLOW_CHECKS)
      CHECK(wrapped_ptr != nullptr);
#endif
      ReleaseInternal(address);
    }
    // We are unable to counteract BanSuperPageFromBRPPool(), called from
    // WrapRawPtr(). We only use one bit per super-page and, thus can't tell if
    // there's more than one associated raw_ptr<T> at a given time. The risk of
    // exhausting the entire address space is minuscule, therefore, we couldn't
    // resist the perf gain of a single relaxed store (in the above mentioned
    // function) over much more expensive two CAS operations, which we'd have to
    // use if we were to un-ban a super-page.
  }

  // Unwraps the pointer, while asserting that memory hasn't been freed. The
  // function is allowed to crash on nullptr.
  template <typename T>
  static ALWAYS_INLINE T* SafelyUnwrapPtrForDereference(T* wrapped_ptr) {
#if DCHECK_IS_ON() || BUILDFLAG(ENABLE_BACKUP_REF_PTR_SLOW_CHECKS)
    uintptr_t address = reinterpret_cast<uintptr_t>(wrapped_ptr);
    if (IsSupportedAndNotNull(address)) {
      CHECK(wrapped_ptr != nullptr);
      CHECK(IsPointeeAlive(address));
    }
#endif
    return wrapped_ptr;
  }

  // Unwraps the pointer, while asserting that memory hasn't been freed. The
  // function must handle nullptr gracefully.
  template <typename T>
  static ALWAYS_INLINE T* SafelyUnwrapPtrForExtraction(T* wrapped_ptr) {
    return wrapped_ptr;
  }

  // Unwraps the pointer, without making an assertion on whether memory was
  // freed or not.
  template <typename T>
  static ALWAYS_INLINE T* UnsafelyUnwrapPtrForComparison(T* wrapped_ptr) {
    return wrapped_ptr;
  }

  // Upcasts the wrapped pointer.
  template <typename To, typename From>
  static ALWAYS_INLINE constexpr To* Upcast(From* wrapped_ptr) {
    static_assert(std::is_convertible<From*, To*>::value,
                  "From must be convertible to To.");
    // Note, this cast may change the address if upcasting to base that lies in
    // the middle of the derived object.
    return wrapped_ptr;
  }

  // Advance the wrapped pointer by |delta| bytes.
  template <typename T>
  static ALWAYS_INLINE T* Advance(T* wrapped_ptr, ptrdiff_t delta_elem) {
#if DCHECK_IS_ON() || BUILDFLAG(ENABLE_BACKUP_REF_PTR_SLOW_CHECKS)
    uintptr_t address = reinterpret_cast<uintptr_t>(wrapped_ptr);
    if (IsSupportedAndNotNull(address))
      CHECK(IsValidDelta(address, delta_elem * sizeof(T)));
#endif
    T* new_wrapped_ptr = WrapRawPtr(wrapped_ptr + delta_elem);
    ReleaseWrappedPtr(wrapped_ptr);
    return new_wrapped_ptr;
  }

  // Returns a copy of a wrapped pointer, without making an assertion on whether
  // memory was freed or not.
  // This method increments the reference count of the allocation slot.
  template <typename T>
  static ALWAYS_INLINE T* Duplicate(T* wrapped_ptr) {
    return WrapRawPtr(wrapped_ptr);
  }

  // This is for accounting only, used by unit tests.
  static ALWAYS_INLINE void IncrementSwapCountForTest() {}
  static ALWAYS_INLINE void IncrementPointerToMemberOperatorCountForTest() {}

 private:
  // We've evaluated several strategies (inline nothing, various parts, or
  // everything in |Wrap()| and |Release()|) using the Speedometer2 benchmark
  // to measure performance. The best results were obtained when only the
  // lightweight |IsManagedByPartitionAllocBRPPool()| check was inlined.
  // Therefore, we've extracted the rest into the functions below and marked
  // them as NOINLINE to prevent unintended LTO effects.
  static BASE_EXPORT NOINLINE void AcquireInternal(uintptr_t address);
  static BASE_EXPORT NOINLINE void ReleaseInternal(uintptr_t address);
  static BASE_EXPORT NOINLINE bool IsPointeeAlive(uintptr_t address);
  static BASE_EXPORT NOINLINE bool IsValidDelta(uintptr_t address,
                                                ptrdiff_t delta_in_bytes);
};

#endif  // BUILDFLAG(USE_BACKUP_REF_PTR)

// Implementation that allows us to detect BackupRefPtr problems in ASan builds.
struct AsanBackupRefPtrImpl {
  // Wraps a pointer.
  template <typename T>
  static ALWAYS_INLINE T* WrapRawPtr(T* ptr) {
    AsanCheckIfValidInstantiation(ptr);
    return ptr;
  }

  // Notifies the allocator when a wrapped pointer is being removed or replaced.
  template <typename T>
  static ALWAYS_INLINE void ReleaseWrappedPtr(T*) {}

  // Unwraps the pointer, while asserting that memory hasn't been freed. The
  // function is allowed to crash on nullptr.
  template <typename T>
  static ALWAYS_INLINE T* SafelyUnwrapPtrForDereference(T* wrapped_ptr) {
    AsanCheckIfValidDereference(wrapped_ptr);
    return wrapped_ptr;
  }

  // Unwraps the pointer, while asserting that memory hasn't been freed. The
  // function must handle nullptr gracefully.
  template <typename T>
  static ALWAYS_INLINE T* SafelyUnwrapPtrForExtraction(T* wrapped_ptr) {
    AsanCheckIfValidExtraction(wrapped_ptr);
    return wrapped_ptr;
  }

  // Unwraps the pointer, without making an assertion on whether memory was
  // freed or not.
  template <typename T>
  static ALWAYS_INLINE T* UnsafelyUnwrapPtrForComparison(T* wrapped_ptr) {
    return wrapped_ptr;
  }

  // Upcasts the wrapped pointer.
  template <typename To, typename From>
  static ALWAYS_INLINE constexpr To* Upcast(From* wrapped_ptr) {
    static_assert(std::is_convertible<From*, To*>::value,
                  "From must be convertible to To.");
    // Note, this cast may change the address if upcasting to base that lies in
    // the middle of the derived object.
    return wrapped_ptr;
  }

  // Advance the wrapped pointer by |delta| bytes.
  template <typename T>
  static ALWAYS_INLINE T* Advance(T* wrapped_ptr, ptrdiff_t delta_elems) {
    return wrapped_ptr + delta_elems;
  }

  // Returns a copy of a wrapped pointer, without making an assertion on whether
  // memory was freed or not.
  template <typename T>
  static ALWAYS_INLINE T* Duplicate(T* wrapped_ptr) {
    return wrapped_ptr;
  }

  // This is for accounting only, used by unit tests.
  static ALWAYS_INLINE void IncrementSwapCountForTest() {}
  static ALWAYS_INLINE void IncrementPointerToMemberOperatorCountForTest() {}

 private:
  static BASE_EXPORT NOINLINE void AsanCheckIfValidInstantiation(
      void const volatile* ptr);
  static BASE_EXPORT NOINLINE void AsanCheckIfValidDereference(
      void const volatile* ptr);
  static BASE_EXPORT NOINLINE void AsanCheckIfValidExtraction(
      void const volatile* ptr);
};

}  // namespace internal

namespace raw_ptr_traits {

// IsSupportedType<T>::value answers whether raw_ptr<T> 1) compiles and 2) is
// always safe at runtime.  Templates that may end up using `raw_ptr<T>` should
// use IsSupportedType to ensure that raw_ptr is not used with unsupported
// types.  As an example, see how base::internal::StorageTraits uses
// IsSupportedType as a condition for using base::internal::UnretainedWrapper
// (which has a `ptr_` field that will become `raw_ptr<T>` after the Big
// Rewrite).
template <typename T, typename SFINAE = void>
struct IsSupportedType {
  static constexpr bool value = true;
};

// raw_ptr<T> is not compatible with function pointer types. Also, they don't
// even need the raw_ptr protection, because they don't point on heap.
template <typename T>
struct IsSupportedType<T, std::enable_if_t<std::is_function<T>::value>> {
  static constexpr bool value = false;
};

// This section excludes some types from raw_ptr<T> to avoid them from being
// used inside base::Unretained in performance sensitive places. These were
// identified from sampling profiler data. See crbug.com/1287151 for more info.
template <>
struct IsSupportedType<cc::Scheduler> {
  static constexpr bool value = false;
};
template <>
struct IsSupportedType<base::internal::DelayTimerBase> {
  static constexpr bool value = false;
};
template <>
struct IsSupportedType<content::responsiveness::Calculator> {
  static constexpr bool value = false;
};

#if __OBJC__
// raw_ptr<T> is not compatible with pointers to Objective-C classes for a
// multitude of reasons. They may fail to compile in many cases, and wouldn't
// work well with tagged pointers. Anyway, Objective-C objects have their own
// way of tracking lifespan, hence don't need the raw_ptr protection as much.
//
// Such pointers are detected by checking if they're convertible to |id| type.
template <typename T>
struct IsSupportedType<T,
                       std::enable_if_t<std::is_convertible<T*, id>::value>> {
  static constexpr bool value = false;
};
#endif  // __OBJC__

#if BUILDFLAG(IS_WIN)
// raw_ptr<HWND__> is unsafe at runtime - if the handle happens to also
// represent a valid pointer into a PartitionAlloc-managed region then it can
// lead to manipulating random memory when treating it as BackupRefPtr
// ref-count.  See also https://crbug.com/1262017.
//
// TODO(https://crbug.com/1262017): Cover other handle types like HANDLE,
// HLOCAL, HINTERNET, or HDEVINFO.  Maybe we should avoid using raw_ptr<T> when
// T=void (as is the case in these handle types).  OTOH, explicit,
// non-template-based raw_ptr<void> should be allowed.  Maybe this can be solved
// by having 2 traits: IsPointeeAlwaysSafe (to be used in templates) and
// IsPointeeUsuallySafe (to be used in the static_assert in raw_ptr).  The
// upside of this approach is that it will safely handle base::Bind closing over
// HANDLE.  The downside of this approach is that base::Bind closing over a
// void* pointer will not get UaF protection.
#define CHROME_WINDOWS_HANDLE_TYPE(name)   \
  template <>                              \
  struct IsSupportedType<name##__, void> { \
    static constexpr bool value = false;   \
  };
#include "base/win/win_handle_types_list.inc"
#undef CHROME_WINDOWS_HANDLE_TYPE
#endif

}  // namespace raw_ptr_traits

// `raw_ptr<T>` is a non-owning smart pointer that has improved memory-safety
// over raw pointers.  It behaves just like a raw pointer on platforms where
// USE_BACKUP_REF_PTR is off, and almost like one when it's on (the main
// difference is that it's zero-initialized and cleared on destruction and
// move). Unlike `std::unique_ptr<T>`, `base::scoped_refptr<T>`, etc., it
// doesn’t manage ownership or lifetime of an allocated object - you are still
// responsible for freeing the object when no longer used, just as you would
// with a raw C++ pointer.
//
// Compared to a raw C++ pointer, on platforms where USE_BACKUP_REF_PTR is on,
// `raw_ptr<T>` incurs additional performance overhead for initialization,
// destruction, and assignment (including `ptr++` and `ptr += ...`).  There is
// no overhead when dereferencing a pointer.
//
// `raw_ptr<T>` is beneficial for security, because it can prevent a significant
// percentage of Use-after-Free (UaF) bugs from being exploitable.  `raw_ptr<T>`
// has limited impact on stability - dereferencing a dangling pointer remains
// Undefined Behavior.  Note that the security protection is not yet enabled by
// default.
//
// raw_ptr<T> is marked as [[gsl::Pointer]] which allows the compiler to catch
// some bugs where the raw_ptr holds a dangling pointer to a temporary object.
// However the [[gsl::Pointer]] analysis expects that such types do not have a
// non-default move constructor/assignment. Thus, it's possible to get an error
// where the pointer is not actually dangling, and have to work around the
// compiler. We have not managed to construct such an example in Chromium yet.
#if BUILDFLAG(USE_BACKUP_REF_PTR)
using RawPtrMayDangle = internal::BackupRefPtrImpl</*AllowDangling=*/true>;
using RawPtrBanDanglingIfSupported =
    internal::BackupRefPtrImpl</*AllowDangling=*/false>;
#elif BUILDFLAG(USE_ASAN_BACKUP_REF_PTR)
using RawPtrMayDangle = internal::AsanBackupRefPtrImpl;
using RawPtrBanDanglingIfSupported = internal::AsanBackupRefPtrImpl;
#elif defined(PA_USE_MTE_CHECKED_PTR_WITH_64_BITS_POINTERS)
using RawPtrMayDangle = internal::MTECheckedPtrImpl<
    internal::MTECheckedPtrImplPartitionAllocSupport>;
using RawPtrBanDanglingIfSupported = internal::MTECheckedPtrImpl<
    internal::MTECheckedPtrImplPartitionAllocSupport>;
#else
using RawPtrMayDangle = internal::RawPtrNoOpImpl;
using RawPtrBanDanglingIfSupported = internal::RawPtrNoOpImpl;
#endif

template <typename T, typename Impl = RawPtrBanDanglingIfSupported>
class TRIVIAL_ABI GSL_POINTER raw_ptr {
 public:
  static_assert(raw_ptr_traits::IsSupportedType<T>::value,
                "raw_ptr<T> doesn't work with this kind of pointee type T");

#if BUILDFLAG(USE_BACKUP_REF_PTR)
  // BackupRefPtr requires a non-trivial default constructor, destructor, etc.
  constexpr ALWAYS_INLINE raw_ptr() noexcept : wrapped_ptr_(nullptr) {}

  ALWAYS_INLINE raw_ptr(const raw_ptr& p) noexcept
      : wrapped_ptr_(Impl::Duplicate(p.wrapped_ptr_)) {}

  ALWAYS_INLINE raw_ptr(raw_ptr&& p) noexcept {
    wrapped_ptr_ = p.wrapped_ptr_;
    p.wrapped_ptr_ = nullptr;
  }

  ALWAYS_INLINE raw_ptr& operator=(const raw_ptr& p) {
    // Duplicate before releasing, in case the pointer is assigned to itself.
    T* new_ptr = Impl::Duplicate(p.wrapped_ptr_);
    Impl::ReleaseWrappedPtr(wrapped_ptr_);
    wrapped_ptr_ = new_ptr;
    return *this;
  }

  ALWAYS_INLINE raw_ptr& operator=(raw_ptr&& p) {
    if (LIKELY(this != &p)) {
      Impl::ReleaseWrappedPtr(wrapped_ptr_);
      wrapped_ptr_ = p.wrapped_ptr_;
      p.wrapped_ptr_ = nullptr;
    }
    return *this;
  }

  ALWAYS_INLINE ~raw_ptr() noexcept {
    Impl::ReleaseWrappedPtr(wrapped_ptr_);
    // Work around external issues where raw_ptr is used after destruction.
    wrapped_ptr_ = nullptr;
  }

#else  // BUILDFLAG(USE_BACKUP_REF_PTR)

  // raw_ptr can be trivially default constructed (leaving |wrapped_ptr_|
  // uninitialized).  This is needed for compatibility with raw pointers.
  //
  // TODO(lukasza): Always initialize |wrapped_ptr_|.  Fix resulting build
  // errors.  Analyze performance impact.
  constexpr ALWAYS_INLINE raw_ptr() noexcept = default;

  // In addition to nullptr_t ctor above, raw_ptr needs to have these
  // as |=default| or |constexpr| to avoid hitting -Wglobal-constructors in
  // cases like this:
  //     struct SomeStruct { int int_field; raw_ptr<int> ptr_field; };
  //     SomeStruct g_global_var = { 123, nullptr };
  ALWAYS_INLINE raw_ptr(const raw_ptr&) noexcept = default;
  ALWAYS_INLINE raw_ptr(raw_ptr&&) noexcept = default;
  ALWAYS_INLINE raw_ptr& operator=(const raw_ptr&) noexcept = default;
  ALWAYS_INLINE raw_ptr& operator=(raw_ptr&&) noexcept = default;

  ALWAYS_INLINE ~raw_ptr() = default;

#endif  // BUILDFLAG(USE_BACKUP_REF_PTR)

  // Deliberately implicit, because raw_ptr is supposed to resemble raw ptr.
  // NOLINTNEXTLINE(google-explicit-constructor)
  constexpr ALWAYS_INLINE raw_ptr(std::nullptr_t) noexcept
      : wrapped_ptr_(nullptr) {}

  // Deliberately implicit, because raw_ptr is supposed to resemble raw ptr.
  // NOLINTNEXTLINE(google-explicit-constructor)
  ALWAYS_INLINE raw_ptr(T* p) noexcept : wrapped_ptr_(Impl::WrapRawPtr(p)) {}

  // Deliberately implicit in order to support implicit upcast.
  template <typename U,
            typename Unused = std::enable_if_t<
                std::is_convertible<U*, T*>::value &&
                !std::is_void<typename std::remove_cv<T>::type>::value>>
  // NOLINTNEXTLINE(google-explicit-constructor)
  ALWAYS_INLINE raw_ptr(const raw_ptr<U, Impl>& ptr) noexcept
      : wrapped_ptr_(
            Impl::Duplicate(Impl::template Upcast<T, U>(ptr.wrapped_ptr_))) {}
  // Deliberately implicit in order to support implicit upcast.
  template <typename U,
            typename Unused = std::enable_if_t<
                std::is_convertible<U*, T*>::value &&
                !std::is_void<typename std::remove_cv<T>::type>::value>>
  // NOLINTNEXTLINE(google-explicit-constructor)
  ALWAYS_INLINE raw_ptr(raw_ptr<U, Impl>&& ptr) noexcept
      : wrapped_ptr_(Impl::template Upcast<T, U>(ptr.wrapped_ptr_)) {
#if BUILDFLAG(USE_BACKUP_REF_PTR)
    ptr.wrapped_ptr_ = nullptr;
#endif
  }

  ALWAYS_INLINE raw_ptr& operator=(std::nullptr_t) noexcept {
    Impl::ReleaseWrappedPtr(wrapped_ptr_);
    wrapped_ptr_ = nullptr;
    return *this;
  }
  ALWAYS_INLINE raw_ptr& operator=(T* p) noexcept {
    Impl::ReleaseWrappedPtr(wrapped_ptr_);
    wrapped_ptr_ = Impl::WrapRawPtr(p);
    return *this;
  }

  // Upcast assignment
  template <typename U,
            typename Unused = std::enable_if_t<
                std::is_convertible<U*, T*>::value &&
                !std::is_void<typename std::remove_cv<T>::type>::value>>
  ALWAYS_INLINE raw_ptr& operator=(const raw_ptr<U, Impl>& ptr) noexcept {
    // Make sure that pointer isn't assigned to itself (look at pointer address,
    // not its value).
#if DCHECK_IS_ON() || BUILDFLAG(ENABLE_BACKUP_REF_PTR_SLOW_CHECKS)
    CHECK(reinterpret_cast<uintptr_t>(this) !=
          reinterpret_cast<uintptr_t>(&ptr));
#endif
    Impl::ReleaseWrappedPtr(wrapped_ptr_);
    wrapped_ptr_ =
        Impl::Duplicate(Impl::template Upcast<T, U>(ptr.wrapped_ptr_));
    return *this;
  }
  template <typename U,
            typename Unused = std::enable_if_t<
                std::is_convertible<U*, T*>::value &&
                !std::is_void<typename std::remove_cv<T>::type>::value>>
  ALWAYS_INLINE raw_ptr& operator=(raw_ptr<U, Impl>&& ptr) noexcept {
    // Make sure that pointer isn't assigned to itself (look at pointer address,
    // not its value).
#if DCHECK_IS_ON() || BUILDFLAG(ENABLE_BACKUP_REF_PTR_SLOW_CHECKS)
    CHECK(reinterpret_cast<uintptr_t>(this) !=
          reinterpret_cast<uintptr_t>(&ptr));
#endif
    Impl::ReleaseWrappedPtr(wrapped_ptr_);
    wrapped_ptr_ = Impl::template Upcast<T, U>(ptr.wrapped_ptr_);
#if BUILDFLAG(USE_BACKUP_REF_PTR)
    ptr.wrapped_ptr_ = nullptr;
#endif
    return *this;
  }

  // Avoid using. The goal of raw_ptr is to be as close to raw pointer as
  // possible, so use it only if absolutely necessary (e.g. for const_cast).
  ALWAYS_INLINE T* get() const { return GetForExtraction(); }

  explicit ALWAYS_INLINE operator bool() const { return !!wrapped_ptr_; }

  template <typename U = T,
            typename Unused = std::enable_if_t<
                !std::is_void<typename std::remove_cv<U>::type>::value>>
  ALWAYS_INLINE U& operator*() const {
    return *GetForDereference();
  }
  ALWAYS_INLINE T* operator->() const { return GetForDereference(); }

  // Disables `(my_raw_ptr->*pmf)(...)` as a workaround for
  // the ICE in GCC parsing the code, reported at
  // https://gcc.gnu.org/bugzilla/show_bug.cgi?id=103455
  template <typename PMF>
  void operator->*(PMF) const = delete;

  // Deliberately implicit, because raw_ptr is supposed to resemble raw ptr.
  // NOLINTNEXTLINE(runtime/explicit)
  ALWAYS_INLINE operator T*() const { return GetForExtraction(); }
  template <typename U>
  explicit ALWAYS_INLINE operator U*() const {
    // This operator may be invoked from static_cast, meaning the types may not
    // be implicitly convertible, hence the need for static_cast here.
    return static_cast<U*>(GetForExtraction());
  }

  ALWAYS_INLINE raw_ptr& operator++() {
    wrapped_ptr_ = Impl::Advance(wrapped_ptr_, 1);
    return *this;
  }
  ALWAYS_INLINE raw_ptr& operator--() {
    wrapped_ptr_ = Impl::Advance(wrapped_ptr_, -1);
    return *this;
  }
  ALWAYS_INLINE raw_ptr operator++(int /* post_increment */) {
    raw_ptr result = *this;
    ++(*this);
    return result;
  }
  ALWAYS_INLINE raw_ptr operator--(int /* post_decrement */) {
    raw_ptr result = *this;
    --(*this);
    return result;
  }
  ALWAYS_INLINE raw_ptr& operator+=(ptrdiff_t delta_elems) {
    wrapped_ptr_ = Impl::Advance(wrapped_ptr_, delta_elems);
    return *this;
  }
  ALWAYS_INLINE raw_ptr& operator-=(ptrdiff_t delta_elems) {
    return *this += -delta_elems;
  }

  // Stop referencing the underlying pointer and free its memory. Compared to
  // raw delete calls, this avoids the raw_ptr to be temporarily dangling
  // during the free operation, which will lead to taking the slower path that
  // involves quarantine.
  ALWAYS_INLINE void ClearAndDelete() noexcept {
#if defined(PA_USE_MTE_CHECKED_PTR_WITH_64_BITS_POINTERS)
    // We cannot directly `delete` a wrapped pointer, since the tag bits
    // atop will lead PA totally astray.
    T* ptr = Impl::SafelyUnwrapPtrForExtraction(wrapped_ptr_);
#else
    T* ptr = wrapped_ptr_;
#endif  // defined(PA_USE_MTE_CHECKED_PTR_WITH_64_BITS_POINTERS)
    operator=(nullptr);
    delete ptr;
  }
  ALWAYS_INLINE void ClearAndDeleteArray() noexcept {
#if defined(PA_USE_MTE_CHECKED_PTR_WITH_64_BITS_POINTERS)
    // We cannot directly `delete` a wrapped pointer, since the tag bits
    // atop will lead PA totally astray.
    T* ptr = Impl::SafelyUnwrapPtrForExtraction(wrapped_ptr_);
#else
    T* ptr = wrapped_ptr_;
#endif  // defined(PA_USE_MTE_CHECKED_PTR_WITH_64_BITS_POINTERS)
    operator=(nullptr);
    delete[] ptr;
  }

  // Comparison operators between raw_ptr and raw_ptr<U>/U*/std::nullptr_t.
  // Strictly speaking, it is not necessary to provide these: the compiler can
  // use the conversion operator implicitly to allow comparisons to fall back to
  // comparisons between raw pointers. However, `operator T*`/`operator U*` may
  // perform safety checks with a higher runtime cost, so to avoid this, provide
  // explicit comparison operators for all combinations of parameters.

  // Comparisons between `raw_ptr`s. This unusual declaration and separate
  // definition below is because `GetForComparison()` is a private method. The
  // more conventional approach of defining a comparison operator between
  // `raw_ptr` and `raw_ptr<U>` in the friend declaration itself does not work,
  // because a comparison operator defined inline would not be allowed to call
  // `raw_ptr<U>`'s private `GetForComparison()` method.
  template <typename U, typename V, typename I>
  friend ALWAYS_INLINE bool operator==(const raw_ptr<U, I>& lhs,
                                       const raw_ptr<V, I>& rhs);
  template <typename U>
  friend ALWAYS_INLINE bool operator!=(const raw_ptr& lhs,
                                       const raw_ptr<U, Impl>& rhs) {
    return !(lhs == rhs);
  }
  template <typename U, typename V, typename I>
  friend ALWAYS_INLINE bool operator<(const raw_ptr<U, I>& lhs,
                                      const raw_ptr<V, I>& rhs);
  template <typename U, typename V, typename I>
  friend ALWAYS_INLINE bool operator>(const raw_ptr<U, I>& lhs,
                                      const raw_ptr<V, I>& rhs);
  template <typename U, typename V, typename I>
  friend ALWAYS_INLINE bool operator<=(const raw_ptr<U, I>& lhs,
                                       const raw_ptr<V, I>& rhs);
  template <typename U, typename V, typename I>
  friend ALWAYS_INLINE bool operator>=(const raw_ptr<U, I>& lhs,
                                       const raw_ptr<V, I>& rhs);

  // Comparisons with U*. These operators also handle the case where the RHS is
  // T*.
  template <typename U>
  friend ALWAYS_INLINE bool operator==(const raw_ptr& lhs, U* rhs) {
    return lhs.GetForComparison() == rhs;
  }
  template <typename U>
  friend ALWAYS_INLINE bool operator!=(const raw_ptr& lhs, U* rhs) {
    return !(lhs == rhs);
  }
  template <typename U>
  friend ALWAYS_INLINE bool operator==(U* lhs, const raw_ptr& rhs) {
    return rhs == lhs;  // Reverse order to call the operator above.
  }
  template <typename U>
  friend ALWAYS_INLINE bool operator!=(U* lhs, const raw_ptr& rhs) {
    return rhs != lhs;  // Reverse order to call the operator above.
  }
  template <typename U>
  friend ALWAYS_INLINE bool operator<(const raw_ptr& lhs, U* rhs) {
    return lhs.GetForComparison() < rhs;
  }
  template <typename U>
  friend ALWAYS_INLINE bool operator<=(const raw_ptr& lhs, U* rhs) {
    return lhs.GetForComparison() <= rhs;
  }
  template <typename U>
  friend ALWAYS_INLINE bool operator>(const raw_ptr& lhs, U* rhs) {
    return lhs.GetForComparison() > rhs;
  }
  template <typename U>
  friend ALWAYS_INLINE bool operator>=(const raw_ptr& lhs, U* rhs) {
    return lhs.GetForComparison() >= rhs;
  }
  template <typename U>
  friend ALWAYS_INLINE bool operator<(U* lhs, const raw_ptr& rhs) {
    return lhs < rhs.GetForComparison();
  }
  template <typename U>
  friend ALWAYS_INLINE bool operator<=(U* lhs, const raw_ptr& rhs) {
    return lhs <= rhs.GetForComparison();
  }
  template <typename U>
  friend ALWAYS_INLINE bool operator>(U* lhs, const raw_ptr& rhs) {
    return lhs > rhs.GetForComparison();
  }
  template <typename U>
  friend ALWAYS_INLINE bool operator>=(U* lhs, const raw_ptr& rhs) {
    return lhs >= rhs.GetForComparison();
  }

  // Comparisons with `std::nullptr_t`.
  friend ALWAYS_INLINE bool operator==(const raw_ptr& lhs, std::nullptr_t) {
    return !lhs;
  }
  friend ALWAYS_INLINE bool operator!=(const raw_ptr& lhs, std::nullptr_t) {
    return !!lhs;  // Use !! otherwise the costly implicit cast will be used.
  }
  friend ALWAYS_INLINE bool operator==(std::nullptr_t, const raw_ptr& rhs) {
    return !rhs;
  }
  friend ALWAYS_INLINE bool operator!=(std::nullptr_t, const raw_ptr& rhs) {
    return !!rhs;  // Use !! otherwise the costly implicit cast will be used.
  }

  friend ALWAYS_INLINE void swap(raw_ptr& lhs, raw_ptr& rhs) noexcept {
    Impl::IncrementSwapCountForTest();
    std::swap(lhs.wrapped_ptr_, rhs.wrapped_ptr_);
  }

  // If T can be serialised into trace, its alias is also
  // serialisable.
  template <class U = T>
  typename perfetto::check_traced_value_support<U>::type WriteIntoTrace(
      perfetto::TracedValue&& context) const {
    perfetto::WriteIntoTracedValue(std::move(context), get());
  }

 private:
  // This getter is meant for situations where the pointer is meant to be
  // dereferenced. It is allowed to crash on nullptr (it may or may not),
  // because it knows that the caller will crash on nullptr.
  ALWAYS_INLINE T* GetForDereference() const {
    return Impl::SafelyUnwrapPtrForDereference(wrapped_ptr_);
  }
  // This getter is meant for situations where the raw pointer is meant to be
  // extracted outside of this class, but not necessarily with an intention to
  // dereference. It mustn't crash on nullptr.
  ALWAYS_INLINE T* GetForExtraction() const {
    return Impl::SafelyUnwrapPtrForExtraction(wrapped_ptr_);
  }
  // This getter is meant *only* for situations where the pointer is meant to be
  // compared (guaranteeing no dereference or extraction outside of this class).
  // Any verifications can and should be skipped for performance reasons.
  ALWAYS_INLINE T* GetForComparison() const {
    return Impl::UnsafelyUnwrapPtrForComparison(wrapped_ptr_);
  }

  T* wrapped_ptr_;

  template <typename U, typename V>
  friend class raw_ptr;
};

template <typename U, typename V, typename I>
ALWAYS_INLINE bool operator==(const raw_ptr<U, I>& lhs,
                              const raw_ptr<V, I>& rhs) {
  return lhs.GetForComparison() == rhs.GetForComparison();
}

template <typename U, typename V, typename I>
ALWAYS_INLINE bool operator<(const raw_ptr<U, I>& lhs,
                             const raw_ptr<V, I>& rhs) {
  return lhs.GetForComparison() < rhs.GetForComparison();
}

template <typename U, typename V, typename I>
ALWAYS_INLINE bool operator>(const raw_ptr<U, I>& lhs,
                             const raw_ptr<V, I>& rhs) {
  return lhs.GetForComparison() > rhs.GetForComparison();
}

template <typename U, typename V, typename I>
ALWAYS_INLINE bool operator<=(const raw_ptr<U, I>& lhs,
                              const raw_ptr<V, I>& rhs) {
  return lhs.GetForComparison() <= rhs.GetForComparison();
}

template <typename U, typename V, typename I>
ALWAYS_INLINE bool operator>=(const raw_ptr<U, I>& lhs,
                              const raw_ptr<V, I>& rhs) {
  return lhs.GetForComparison() >= rhs.GetForComparison();
}

}  // namespace base

using base::raw_ptr;

// DisableDanglingPtrDetection option for raw_ptr annotates
// "intentional-and-safe" dangling pointers. It is meant to be used at the
// margin, only if there is no better way to re-architecture the code.
//
// Usage:
// raw_ptr<T, DisableDanglingPtrDetection> dangling_ptr;
//
// When using it, please provide a justification about what guarantees it will
// never be dereferenced after becoming dangling.
using DisableDanglingPtrDetection = base::RawPtrMayDangle;

namespace std {

// Override so set/map lookups do not create extra raw_ptr. This also allows
// dangling pointers to be used for lookup.
template <typename T, typename I>
struct less<raw_ptr<T, I>> {
  using is_transparent = void;

  bool operator()(const raw_ptr<T, I>& lhs, const raw_ptr<T, I>& rhs) const {
    return lhs < rhs;
  }

  bool operator()(T* lhs, const raw_ptr<T, I>& rhs) const { return lhs < rhs; }

  bool operator()(const raw_ptr<T, I>& lhs, T* rhs) const { return lhs < rhs; }
};

}  // namespace std

#endif  // BASE_MEMORY_RAW_PTR_H_

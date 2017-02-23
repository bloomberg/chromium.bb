/*
 *  Copyright (C) 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public License
 *  along with this library; see the file COPYING.LIB.  If not, write to
 *  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 *
 */

#ifndef WTF_Vector_h
#define WTF_Vector_h

#include "wtf/Alignment.h"
#include "wtf/ConditionalDestructor.h"
#include "wtf/ContainerAnnotations.h"
#include "wtf/Noncopyable.h"
#include "wtf/NotFound.h"
#include "wtf/StdLibExtras.h"
#include "wtf/VectorTraits.h"
#include "wtf/allocator/PartitionAllocator.h"
#include <algorithm>
#include <initializer_list>
#include <iterator>
#include <string.h>
#include <utility>

// For ASAN builds, disable inline buffers completely as they cause various
// issues.
#ifdef ANNOTATE_CONTIGUOUS_CONTAINER
#define INLINE_CAPACITY 0
#else
#define INLINE_CAPACITY inlineCapacity
#endif

namespace WTF {

#if defined(MEMORY_SANITIZER_INITIAL_SIZE)
static const size_t kInitialVectorSize = 1;
#else
#ifndef WTF_VECTOR_INITIAL_SIZE
#define WTF_VECTOR_INITIAL_SIZE 4
#endif
static const size_t kInitialVectorSize = WTF_VECTOR_INITIAL_SIZE;
#endif

template <typename T, size_t inlineBuffer, typename Allocator>
class Deque;

//
// Vector Traits
//

// Bunch of traits for Vector are defined here, with which you can customize
// Vector's behavior. In most cases the default traits are appropriate, so you
// usually don't have to specialize those traits by yourself.
//
// The behavior of the implementation below can be controlled by VectorTraits.
// If you want to change the behavior of your type, take a look at VectorTraits
// (defined in VectorTraits.h), too.

template <bool needsDestruction, typename T>
struct VectorDestructor;

template <typename T>
struct VectorDestructor<false, T> {
  STATIC_ONLY(VectorDestructor);
  static void destruct(T*, T*) {}
};

template <typename T>
struct VectorDestructor<true, T> {
  STATIC_ONLY(VectorDestructor);
  static void destruct(T* begin, T* end) {
    for (T* cur = begin; cur != end; ++cur)
      cur->~T();
  }
};

template <bool unusedSlotsMustBeZeroed, typename T>
struct VectorUnusedSlotClearer;

template <typename T>
struct VectorUnusedSlotClearer<false, T> {
  STATIC_ONLY(VectorUnusedSlotClearer);
  static void clear(T*, T*) {}
#if DCHECK_IS_ON()
  static void checkCleared(const T*, const T*) {}
#endif
};

template <typename T>
struct VectorUnusedSlotClearer<true, T> {
  STATIC_ONLY(VectorUnusedSlotClearer);
  static void clear(T* begin, T* end) {
    memset(reinterpret_cast<void*>(begin), 0, sizeof(T) * (end - begin));
  }

#if DCHECK_IS_ON()
  static void checkCleared(const T* begin, const T* end) {
    const unsigned char* unusedArea =
        reinterpret_cast<const unsigned char*>(begin);
    const unsigned char* endAddress =
        reinterpret_cast<const unsigned char*>(end);
    DCHECK_GE(endAddress, unusedArea);
    for (int i = 0; i < endAddress - unusedArea; ++i)
      DCHECK(!unusedArea[i]);
  }
#endif
};

template <bool canInitializeWithMemset, typename T>
struct VectorInitializer;

template <typename T>
struct VectorInitializer<false, T> {
  STATIC_ONLY(VectorInitializer);
  static void initialize(T* begin, T* end) {
    for (T* cur = begin; cur != end; ++cur)
      new (NotNull, cur) T;
  }
};

template <typename T>
struct VectorInitializer<true, T> {
  STATIC_ONLY(VectorInitializer);
  static void initialize(T* begin, T* end) {
    memset(begin, 0,
           reinterpret_cast<char*>(end) - reinterpret_cast<char*>(begin));
  }
};

template <bool canMoveWithMemcpy, typename T>
struct VectorMover;

template <typename T>
struct VectorMover<false, T> {
  STATIC_ONLY(VectorMover);
  static void move(T* src, T* srcEnd, T* dst) {
    while (src != srcEnd) {
      new (NotNull, dst) T(std::move(*src));
      src->~T();
      ++dst;
      ++src;
    }
  }
  static void moveOverlapping(T* src, T* srcEnd, T* dst) {
    if (src > dst) {
      move(src, srcEnd, dst);
    } else {
      T* dstEnd = dst + (srcEnd - src);
      while (src != srcEnd) {
        --srcEnd;
        --dstEnd;
        new (NotNull, dstEnd) T(std::move(*srcEnd));
        srcEnd->~T();
      }
    }
  }
  static void swap(T* src, T* srcEnd, T* dst) {
    std::swap_ranges(src, srcEnd, dst);
  }
};

template <typename T>
struct VectorMover<true, T> {
  STATIC_ONLY(VectorMover);
  static void move(const T* src, const T* srcEnd, T* dst) {
    if (LIKELY(dst && src))
      memcpy(dst, src, reinterpret_cast<const char*>(srcEnd) -
                           reinterpret_cast<const char*>(src));
  }
  static void moveOverlapping(const T* src, const T* srcEnd, T* dst) {
    if (LIKELY(dst && src))
      memmove(dst, src, reinterpret_cast<const char*>(srcEnd) -
                            reinterpret_cast<const char*>(src));
  }
  static void swap(T* src, T* srcEnd, T* dst) {
    std::swap_ranges(reinterpret_cast<char*>(src),
                     reinterpret_cast<char*>(srcEnd),
                     reinterpret_cast<char*>(dst));
  }
};

template <bool canCopyWithMemcpy, typename T>
struct VectorCopier;

template <typename T>
struct VectorCopier<false, T> {
  STATIC_ONLY(VectorCopier);
  template <typename U>
  static void uninitializedCopy(const U* src, const U* srcEnd, T* dst) {
    while (src != srcEnd) {
      new (NotNull, dst) T(*src);
      ++dst;
      ++src;
    }
  }
};

template <typename T>
struct VectorCopier<true, T> {
  STATIC_ONLY(VectorCopier);
  static void uninitializedCopy(const T* src, const T* srcEnd, T* dst) {
    if (LIKELY(dst && src))
      memcpy(dst, src, reinterpret_cast<const char*>(srcEnd) -
                           reinterpret_cast<const char*>(src));
  }
  template <typename U>
  static void uninitializedCopy(const U* src, const U* srcEnd, T* dst) {
    VectorCopier<false, T>::uninitializedCopy(src, srcEnd, dst);
  }
};

template <bool canFillWithMemset, typename T>
struct VectorFiller;

template <typename T>
struct VectorFiller<false, T> {
  STATIC_ONLY(VectorFiller);
  static void uninitializedFill(T* dst, T* dstEnd, const T& val) {
    while (dst != dstEnd) {
      new (NotNull, dst) T(val);
      ++dst;
    }
  }
};

template <typename T>
struct VectorFiller<true, T> {
  STATIC_ONLY(VectorFiller);
  static void uninitializedFill(T* dst, T* dstEnd, const T& val) {
    static_assert(sizeof(T) == sizeof(char), "size of type should be one");
#if COMPILER(GCC) && defined(_FORTIFY_SOURCE)
    if (!__builtin_constant_p(dstEnd - dst) || (!(dstEnd - dst)))
      memset(dst, val, dstEnd - dst);
#else
    memset(dst, val, dstEnd - dst);
#endif
  }
};

template <bool canCompareWithMemcmp, typename T>
struct VectorComparer;

template <typename T>
struct VectorComparer<false, T> {
  STATIC_ONLY(VectorComparer);
  static bool compare(const T* a, const T* b, size_t size) {
    DCHECK(a);
    DCHECK(b);
    return std::equal(a, a + size, b);
  }
};

template <typename T>
struct VectorComparer<true, T> {
  STATIC_ONLY(VectorComparer);
  static bool compare(const T* a, const T* b, size_t size) {
    DCHECK(a);
    DCHECK(b);
    return memcmp(a, b, sizeof(T) * size) == 0;
  }
};

template <typename T>
struct VectorElementComparer {
  STATIC_ONLY(VectorElementComparer);
  template <typename U>
  static bool compareElement(const T& left, const U& right) {
    return left == right;
  }
};

template <typename T>
struct VectorElementComparer<std::unique_ptr<T>> {
  STATIC_ONLY(VectorElementComparer);
  template <typename U>
  static bool compareElement(const std::unique_ptr<T>& left, const U& right) {
    return left.get() == right;
  }
};

// A collection of all the traits used by Vector. This is basically an
// implementation detail of Vector, and you probably don't want to change this.
// If you want to customize Vector's behavior, you should specialize
// VectorTraits instead (defined in VectorTraits.h).
template <typename T>
struct VectorTypeOperations {
  STATIC_ONLY(VectorTypeOperations);
  static void destruct(T* begin, T* end) {
    VectorDestructor<VectorTraits<T>::needsDestruction, T>::destruct(begin,
                                                                     end);
  }

  static void initialize(T* begin, T* end) {
    VectorInitializer<VectorTraits<T>::canInitializeWithMemset, T>::initialize(
        begin, end);
  }

  static void move(T* src, T* srcEnd, T* dst) {
    VectorMover<VectorTraits<T>::canMoveWithMemcpy, T>::move(src, srcEnd, dst);
  }

  static void moveOverlapping(T* src, T* srcEnd, T* dst) {
    VectorMover<VectorTraits<T>::canMoveWithMemcpy, T>::moveOverlapping(
        src, srcEnd, dst);
  }

  static void swap(T* src, T* srcEnd, T* dst) {
    VectorMover<VectorTraits<T>::canMoveWithMemcpy, T>::swap(src, srcEnd, dst);
  }

  static void uninitializedCopy(const T* src, const T* srcEnd, T* dst) {
    VectorCopier<VectorTraits<T>::canCopyWithMemcpy, T>::uninitializedCopy(
        src, srcEnd, dst);
  }

  static void uninitializedFill(T* dst, T* dstEnd, const T& val) {
    VectorFiller<VectorTraits<T>::canFillWithMemset, T>::uninitializedFill(
        dst, dstEnd, val);
  }

  static bool compare(const T* a, const T* b, size_t size) {
    return VectorComparer<VectorTraits<T>::canCompareWithMemcmp, T>::compare(
        a, b, size);
  }

  template <typename U>
  static bool compareElement(const T& left, U&& right) {
    return VectorElementComparer<T>::compareElement(left,
                                                    std::forward<U>(right));
  }
};

//
// VectorBuffer
//

// VectorBuffer is an implementation detail of Vector and Deque. It manages
// Vector's underlying buffer, and does operations like allocation or
// expansion.
//
// Not meant for general consumption.

template <typename T, bool hasInlineCapacity, typename Allocator>
class VectorBufferBase {
  WTF_MAKE_NONCOPYABLE(VectorBufferBase);
  DISALLOW_NEW();

 public:
  void allocateBuffer(size_t newCapacity) {
    DCHECK(newCapacity);
    DCHECK_LE(newCapacity,
              Allocator::template maxElementCountInBackingStore<T>());
    size_t sizeToAllocate = allocationSize(newCapacity);
    if (hasInlineCapacity)
      m_buffer =
          Allocator::template allocateInlineVectorBacking<T>(sizeToAllocate);
    else
      m_buffer = Allocator::template allocateVectorBacking<T>(sizeToAllocate);
    m_capacity = sizeToAllocate / sizeof(T);
  }

  void allocateExpandedBuffer(size_t newCapacity) {
    DCHECK(newCapacity);
    size_t sizeToAllocate = allocationSize(newCapacity);
    if (hasInlineCapacity)
      m_buffer =
          Allocator::template allocateInlineVectorBacking<T>(sizeToAllocate);
    else
      m_buffer =
          Allocator::template allocateExpandedVectorBacking<T>(sizeToAllocate);
    m_capacity = sizeToAllocate / sizeof(T);
  }

  size_t allocationSize(size_t capacity) const {
    return Allocator::template quantizedSize<T>(capacity);
  }

  T* buffer() { return m_buffer; }
  const T* buffer() const { return m_buffer; }
  size_t capacity() const { return m_capacity; }

  void clearUnusedSlots(T* from, T* to) {
    // If the vector backing is garbage-collected and needs tracing or
    // finalizing, we clear out the unused slots so that the visitor or the
    // finalizer does not cause a problem when visiting the unused slots.
    VectorUnusedSlotClearer<
        Allocator::isGarbageCollected &&
            (VectorTraits<T>::needsDestruction ||
             IsTraceableInCollectionTrait<VectorTraits<T>>::value),
        T>::clear(from, to);
  }

  void checkUnusedSlots(const T* from, const T* to) {
#if DCHECK_IS_ON() && !defined(ANNOTATE_CONTIGUOUS_CONTAINER)
    VectorUnusedSlotClearer<
        Allocator::isGarbageCollected &&
            (VectorTraits<T>::needsDestruction ||
             IsTraceableInCollectionTrait<VectorTraits<T>>::value),
        T>::checkCleared(from, to);
#endif
  }

  // |end| is exclusive, a la STL.
  struct OffsetRange final {
    OffsetRange() : begin(0), end(0) {}
    explicit OffsetRange(size_t begin, size_t end) : begin(begin), end(end) {
      DCHECK_LE(begin, end);
    }
    bool empty() const { return begin == end; }
    size_t begin;
    size_t end;
  };

 protected:
  VectorBufferBase() : m_buffer(nullptr), m_capacity(0) {}

  VectorBufferBase(T* buffer, size_t capacity)
      : m_buffer(buffer), m_capacity(capacity) {}

  T* m_buffer;
  unsigned m_capacity;
  unsigned m_size;
};

template <typename T,
          size_t inlineCapacity,
          typename Allocator = PartitionAllocator>
class VectorBuffer;

template <typename T, typename Allocator>
class VectorBuffer<T, 0, Allocator>
    : protected VectorBufferBase<T, false, Allocator> {
 private:
  using Base = VectorBufferBase<T, false, Allocator>;

 public:
  using OffsetRange = typename Base::OffsetRange;

  VectorBuffer() {}

  explicit VectorBuffer(size_t capacity) {
    // Calling malloc(0) might take a lock and may actually do an allocation
    // on some systems.
    if (capacity)
      allocateBuffer(capacity);
  }

  void destruct() {
    deallocateBuffer(m_buffer);
    m_buffer = nullptr;
  }

  void deallocateBuffer(T* bufferToDeallocate) {
    Allocator::freeVectorBacking(bufferToDeallocate);
  }

  bool expandBuffer(size_t newCapacity) {
    size_t sizeToAllocate = allocationSize(newCapacity);
    if (Allocator::expandVectorBacking(m_buffer, sizeToAllocate)) {
      m_capacity = sizeToAllocate / sizeof(T);
      return true;
    }
    return false;
  }

  inline bool shrinkBuffer(size_t newCapacity) {
    DCHECK_LT(newCapacity, capacity());
    size_t sizeToAllocate = allocationSize(newCapacity);
    if (Allocator::shrinkVectorBacking(m_buffer, allocationSize(capacity()),
                                       sizeToAllocate)) {
      m_capacity = sizeToAllocate / sizeof(T);
      return true;
    }
    return false;
  }

  void resetBufferPointer() {
    m_buffer = nullptr;
    m_capacity = 0;
  }

  // See the other specialization for the meaning of |thisHole| and |otherHole|.
  // They are irrelevant in this case.
  void swapVectorBuffer(VectorBuffer<T, 0, Allocator>& other,
                        OffsetRange thisHole,
                        OffsetRange otherHole) {
    static_assert(VectorTraits<T>::canSwapUsingCopyOrMove,
                  "Cannot swap HeapVectors of TraceWrapperMembers.");

    std::swap(m_buffer, other.m_buffer);
    std::swap(m_capacity, other.m_capacity);
    std::swap(m_size, other.m_size);
  }

  using Base::allocateBuffer;
  using Base::allocationSize;

  using Base::buffer;
  using Base::capacity;

  using Base::clearUnusedSlots;
  using Base::checkUnusedSlots;

  bool hasOutOfLineBuffer() const {
    // When inlineCapacity is 0 we have an out of line buffer if we have a
    // buffer.
    return buffer();
  }

  T** bufferSlot() { return &m_buffer; }

 protected:
  using Base::m_size;

 private:
  using Base::m_buffer;
  using Base::m_capacity;
};

template <typename T, size_t inlineCapacity, typename Allocator>
class VectorBuffer : protected VectorBufferBase<T, true, Allocator> {
  WTF_MAKE_NONCOPYABLE(VectorBuffer);

 private:
  using Base = VectorBufferBase<T, true, Allocator>;

 public:
  using OffsetRange = typename Base::OffsetRange;

  VectorBuffer() : Base(inlineBuffer(), inlineCapacity) {}

  explicit VectorBuffer(size_t capacity)
      : Base(inlineBuffer(), inlineCapacity) {
    if (capacity > inlineCapacity)
      Base::allocateBuffer(capacity);
  }

  void destruct() {
    deallocateBuffer(m_buffer);
    m_buffer = nullptr;
  }

  NEVER_INLINE void reallyDeallocateBuffer(T* bufferToDeallocate) {
    Allocator::freeInlineVectorBacking(bufferToDeallocate);
  }

  void deallocateBuffer(T* bufferToDeallocate) {
    if (UNLIKELY(bufferToDeallocate != inlineBuffer()))
      reallyDeallocateBuffer(bufferToDeallocate);
  }

  bool expandBuffer(size_t newCapacity) {
    DCHECK_GT(newCapacity, inlineCapacity);
    if (m_buffer == inlineBuffer())
      return false;

    size_t sizeToAllocate = allocationSize(newCapacity);
    if (Allocator::expandInlineVectorBacking(m_buffer, sizeToAllocate)) {
      m_capacity = sizeToAllocate / sizeof(T);
      return true;
    }
    return false;
  }

  inline bool shrinkBuffer(size_t newCapacity) {
    DCHECK_LT(newCapacity, capacity());
    if (newCapacity <= inlineCapacity) {
      // We need to switch to inlineBuffer.  Vector::shrinkCapacity will
      // handle it.
      return false;
    }
    DCHECK_NE(m_buffer, inlineBuffer());
    size_t newSize = allocationSize(newCapacity);
    if (!Allocator::shrinkInlineVectorBacking(
            m_buffer, allocationSize(capacity()), newSize))
      return false;
    m_capacity = newSize / sizeof(T);
    return true;
  }

  void resetBufferPointer() {
    m_buffer = inlineBuffer();
    m_capacity = inlineCapacity;
  }

  void allocateBuffer(size_t newCapacity) {
    // FIXME: This should DCHECK(!m_buffer) to catch misuse/leaks.
    if (newCapacity > inlineCapacity)
      Base::allocateBuffer(newCapacity);
    else
      resetBufferPointer();
  }

  void allocateExpandedBuffer(size_t newCapacity) {
    if (newCapacity > inlineCapacity)
      Base::allocateExpandedBuffer(newCapacity);
    else
      resetBufferPointer();
  }

  size_t allocationSize(size_t capacity) const {
    if (capacity <= inlineCapacity)
      return m_inlineBufferSize;
    return Base::allocationSize(capacity);
  }

  // Swap two vector buffers, both of which have the same non-zero inline
  // capacity.
  //
  // If the data is in an out-of-line buffer, we can just pass the pointers
  // across the two buffers.  If the data is in an inline buffer, we need to
  // either swap or move each element, depending on whether each slot is
  // occupied or not.
  //
  // Further complication comes from the fact that VectorBuffer is also used as
  // the backing store of a Deque.  Deque allocates the objects like a ring
  // buffer, so there may be a "hole" (unallocated region) in the middle of the
  // buffer. This function assumes elements in a range [m_buffer, m_buffer +
  // m_size) are all allocated except for elements within |thisHole|. The same
  // applies for |other.m_buffer| and |otherHole|.
  void swapVectorBuffer(VectorBuffer<T, inlineCapacity, Allocator>& other,
                        OffsetRange thisHole,
                        OffsetRange otherHole) {
    using TypeOperations = VectorTypeOperations<T>;

    static_assert(VectorTraits<T>::canSwapUsingCopyOrMove,
                  "Cannot swap HeapVectors of TraceWrapperMembers.");

    if (buffer() != inlineBuffer() && other.buffer() != other.inlineBuffer()) {
      // The easiest case: both buffers are non-inline. We just need to swap the
      // pointers.
      std::swap(m_buffer, other.m_buffer);
      std::swap(m_capacity, other.m_capacity);
      std::swap(m_size, other.m_size);
      return;
    }

    Allocator::enterGCForbiddenScope();

    // Otherwise, we at least need to move some elements from one inline buffer
    // to another.
    //
    // Terminology: "source" is a place from which elements are copied, and
    // "destination" is a place to which elements are copied. thisSource or
    // otherSource can be empty (represented by nullptr) when this range or
    // other range is in an out-of-line buffer.
    //
    // We first record which range needs to get moved and where elements in such
    // a range will go. Elements in an inline buffer will go to the other
    // buffer's inline buffer. Elements in an out-of-line buffer won't move,
    // because we can just swap pointers of out-of-line buffers.
    T* thisSourceBegin = nullptr;
    size_t thisSourceSize = 0;
    T* thisDestinationBegin = nullptr;
    if (buffer() == inlineBuffer()) {
      thisSourceBegin = buffer();
      thisSourceSize = m_size;
      thisDestinationBegin = other.inlineBuffer();
      if (!thisHole.empty()) {  // Sanity check.
        DCHECK_LT(thisHole.begin, thisHole.end);
        DCHECK_LE(thisHole.end, thisSourceSize);
      }
    } else {
      // We don't need the hole information for an out-of-line buffer.
      thisHole.begin = thisHole.end = 0;
    }
    T* otherSourceBegin = nullptr;
    size_t otherSourceSize = 0;
    T* otherDestinationBegin = nullptr;
    if (other.buffer() == other.inlineBuffer()) {
      otherSourceBegin = other.buffer();
      otherSourceSize = other.m_size;
      otherDestinationBegin = inlineBuffer();
      if (!otherHole.empty()) {
        DCHECK_LT(otherHole.begin, otherHole.end);
        DCHECK_LE(otherHole.end, otherSourceSize);
      }
    } else {
      otherHole.begin = otherHole.end = 0;
    }

    // Next, we mutate members and do other bookkeeping. We do pointer swapping
    // (for out-of-line buffers) here if we can. From now on, don't assume
    // buffer() or capacity() maintains their original values.
    std::swap(m_capacity, other.m_capacity);
    if (thisSourceBegin &&
        !otherSourceBegin) {  // Our buffer is inline, theirs is not.
      DCHECK_EQ(buffer(), inlineBuffer());
      DCHECK_NE(other.buffer(), other.inlineBuffer());
      ANNOTATE_DELETE_BUFFER(m_buffer, inlineCapacity, m_size);
      m_buffer = other.buffer();
      other.m_buffer = other.inlineBuffer();
      std::swap(m_size, other.m_size);
      ANNOTATE_NEW_BUFFER(other.m_buffer, inlineCapacity, other.m_size);
    } else if (!thisSourceBegin &&
               otherSourceBegin) {  // Their buffer is inline, ours is not.
      DCHECK_NE(buffer(), inlineBuffer());
      DCHECK_EQ(other.buffer(), other.inlineBuffer());
      ANNOTATE_DELETE_BUFFER(other.m_buffer, inlineCapacity, other.m_size);
      other.m_buffer = buffer();
      m_buffer = inlineBuffer();
      std::swap(m_size, other.m_size);
      ANNOTATE_NEW_BUFFER(m_buffer, inlineCapacity, m_size);
    } else {  // Both buffers are inline.
      DCHECK(thisSourceBegin);
      DCHECK(otherSourceBegin);
      DCHECK_EQ(buffer(), inlineBuffer());
      DCHECK_EQ(other.buffer(), other.inlineBuffer());
      ANNOTATE_CHANGE_SIZE(m_buffer, inlineCapacity, m_size, other.m_size);
      ANNOTATE_CHANGE_SIZE(other.m_buffer, inlineCapacity, other.m_size,
                           m_size);
      std::swap(m_size, other.m_size);
    }

    // We are ready to move elements. We determine an action for each "section",
    // which is a contiguous range such that all elements in the range are
    // treated similarly.
    size_t sectionBegin = 0;
    while (sectionBegin < inlineCapacity) {
      // To determine the end of this section, we list up all the boundaries
      // where the "occupiedness" may change.
      size_t sectionEnd = inlineCapacity;
      if (thisSourceBegin && sectionBegin < thisSourceSize)
        sectionEnd = std::min(sectionEnd, thisSourceSize);
      if (!thisHole.empty() && sectionBegin < thisHole.begin)
        sectionEnd = std::min(sectionEnd, thisHole.begin);
      if (!thisHole.empty() && sectionBegin < thisHole.end)
        sectionEnd = std::min(sectionEnd, thisHole.end);
      if (otherSourceBegin && sectionBegin < otherSourceSize)
        sectionEnd = std::min(sectionEnd, otherSourceSize);
      if (!otherHole.empty() && sectionBegin < otherHole.begin)
        sectionEnd = std::min(sectionEnd, otherHole.begin);
      if (!otherHole.empty() && sectionBegin < otherHole.end)
        sectionEnd = std::min(sectionEnd, otherHole.end);

      DCHECK_LT(sectionBegin, sectionEnd);

      // Is the |sectionBegin|-th element of |thisSource| occupied?
      bool thisOccupied = false;
      if (thisSourceBegin && sectionBegin < thisSourceSize) {
        // Yes, it's occupied, unless the position is in a hole.
        if (thisHole.empty() || sectionBegin < thisHole.begin ||
            sectionBegin >= thisHole.end)
          thisOccupied = true;
      }
      bool otherOccupied = false;
      if (otherSourceBegin && sectionBegin < otherSourceSize) {
        if (otherHole.empty() || sectionBegin < otherHole.begin ||
            sectionBegin >= otherHole.end)
          otherOccupied = true;
      }

      if (thisOccupied && otherOccupied) {
        // Both occupied; swap them. In this case, one's destination must be the
        // other's source (i.e. both ranges are in inline buffers).
        DCHECK_EQ(thisDestinationBegin, otherSourceBegin);
        DCHECK_EQ(otherDestinationBegin, thisSourceBegin);
        TypeOperations::swap(thisSourceBegin + sectionBegin,
                             thisSourceBegin + sectionEnd,
                             otherSourceBegin + sectionBegin);
      } else if (thisOccupied) {
        // Move from ours to theirs.
        TypeOperations::move(thisSourceBegin + sectionBegin,
                             thisSourceBegin + sectionEnd,
                             thisDestinationBegin + sectionBegin);
        Base::clearUnusedSlots(thisSourceBegin + sectionBegin,
                               thisSourceBegin + sectionEnd);
      } else if (otherOccupied) {
        // Move from theirs to ours.
        TypeOperations::move(otherSourceBegin + sectionBegin,
                             otherSourceBegin + sectionEnd,
                             otherDestinationBegin + sectionBegin);
        Base::clearUnusedSlots(otherSourceBegin + sectionBegin,
                               otherSourceBegin + sectionEnd);
      } else {
        // Both empty; nothing to do.
      }

      sectionBegin = sectionEnd;
    }

    Allocator::leaveGCForbiddenScope();
  }

  using Base::buffer;
  using Base::capacity;

  bool hasOutOfLineBuffer() const {
    return buffer() && buffer() != inlineBuffer();
  }

  T** bufferSlot() { return &m_buffer; }

 protected:
  using Base::m_size;

 private:
  using Base::m_buffer;
  using Base::m_capacity;

  static const size_t m_inlineBufferSize = inlineCapacity * sizeof(T);
  T* inlineBuffer() { return reinterpret_cast_ptr<T*>(m_inlineBuffer.buffer); }
  const T* inlineBuffer() const {
    return reinterpret_cast_ptr<const T*>(m_inlineBuffer.buffer);
  }

  AlignedBuffer<m_inlineBufferSize, WTF_ALIGN_OF(T)> m_inlineBuffer;
  template <typename U, size_t inlineBuffer, typename V>
  friend class Deque;
};

//
// Vector
//

// Vector is a container that works just like std::vector. WTF's Vector has
// several extra functionalities: inline buffer, behavior customization via
// traits, and Oilpan support. Those are explained in the sections below.
//
// Vector is the most basic container, which stores its element in a contiguous
// buffer. The buffer is expanded automatically when necessary. The elements
// are automatically moved to the new buffer. This event is called a
// reallocation. A reallocation takes O(N)-time (N = number of elements), but
// its occurrences are rare, so its time cost should not be significant,
// compared to the time cost of other operations to the vector.
//
// Time complexity of key operations is as follows:
//
//     * Indexed access -- O(1)
//     * Insertion or removal of an element at the end -- amortized O(1)
//     * Other insertion or removal -- O(N)
//     * Swapping with another vector -- O(1)
//
// 1. Iterator invalidation semantics
//
// Vector provides STL-compatible iterators and reverse iterators. Iterators
// are _invalidated_ on certain occasions. Reading an invalidated iterator
// causes undefined behavior.
//
// Iterators are invalidated on the following situations:
//
//     * When a reallocation happens on a vector, all the iterators for that
//       vector will be invalidated.
//     * Some member functions invalidate part of the existing iterators for
//       the vector; see comments on the individual functions.
//     * [Oilpan only] Heap compaction invalidates all the iterators for any
//       HeapVectors. This means you can only store an iterator on stack, as
//       a local variable.
//
// In this context, pointers or references to an element of a Vector are
// essentially equivalent to iterators, in that they also become invalid
// whenever corresponding iterators are invalidated.
//
// 2. Inline buffer
//
// Vectors may have an _inline buffer_. An inline buffer is a storage area
// that is contained in the vector itself, along with other metadata like
// m_size. It is used as a storage space when the vector's elements fit in
// that space. If the inline buffer becomes full and further space is
// necessary, an out-of-line buffer is allocated in the heap, and it will
// take over the role of the inline buffer.
//
// The existence of an inline buffer is indicated by non-zero |inlineCapacity|
// template argument. The value represents the number of elements that can be
// stored in the inline buffer. Zero |inlineCapacity| means the vector has no
// inline buffer.
//
// An inline buffer increases the size of the Vector instances, and, in trade
// for that, it gives you several performance benefits, as long as the number
// of elements do not exceed |inlineCapacity|:
//
//     * No heap allocation will be made.
//     * Memory locality will improve.
//
// Generally, having an inline buffer is useful for vectors that (1) are
// frequently accessed or modified, and (2) contain only a few elements at
// most.
//
// 3. Behavior customization
//
// You usually do not need to customize Vector's behavior, since the default
// behavior is appropriate for normal usage. The behavior is controlled by
// VectorTypeOperations traits template above. Read VectorTypeOperations
// and VectorTraits if you want to change the behavior for your types (i.e.
// if you really want faster vector operations).
//
// The default traits basically do the following:
//
//     * Skip constructor call and fill zeros with memset for simple types;
//     * Skip destructor call for simple types;
//     * Copy or move by memcpy for simple types; and
//     * Customize the comparisons for smart pointer types, so you can look
//       up a std::unique_ptr<T> element with a raw pointer, for instance.
//
// 4. Oilpan
//
// If you want to store garbage collected objects in Vector, (1) use HeapVector
// (defined in HeapAllocator.h) instead of Vector, and (2) make sure your
// garbage-collected type is wrapped with Member, like:
//
//     HeapVector<Member<Node>> nodes;
//
// Unlike normal garbage-collected objects, a HeapVector object itself is
// NOT a garbage-collected object, but its backing buffer is allocated in
// Oilpan heap, and it may still carry garbage-collected objects.
//
// Even though a HeapVector object is not garbage-collected, you still need
// to trace it, if you stored it in your class. Also, you can allocate it
// as a local variable. This is useful when you want to build a vector locally
// and put it in an on-heap vector with swap().
//
// Also, heap compaction, which may happen at any time when Blink code is not
// running (i.e. Blink code does not appear in the call stack), may invalidate
// existing iterators for any HeapVectors. So, essentially, you should always
// allocate an iterator on stack (as a local variable), and you should not
// store iterators in another heap object.

template <typename T,
          size_t inlineCapacity = 0,
          typename Allocator = PartitionAllocator>
class Vector
    : private VectorBuffer<T, INLINE_CAPACITY, Allocator>,
      // Heap-allocated vectors with no inlineCapacity never need a destructor.
      public ConditionalDestructor<Vector<T, INLINE_CAPACITY, Allocator>,
                                   (INLINE_CAPACITY == 0) &&
                                       Allocator::isGarbageCollected> {
  USE_ALLOCATOR(Vector, Allocator);
  using Base = VectorBuffer<T, INLINE_CAPACITY, Allocator>;
  using TypeOperations = VectorTypeOperations<T>;
  using OffsetRange = typename Base::OffsetRange;

 public:
  using ValueType = T;
  using value_type = T;

  using iterator = T*;
  using const_iterator = const T*;
  using reverse_iterator = std::reverse_iterator<iterator>;
  using const_reverse_iterator = std::reverse_iterator<const_iterator>;

  // Create an empty vector.
  inline Vector();
  // Create a vector containing the specified number of default-initialized
  // elements.
  inline explicit Vector(size_t);
  // Create a vector containing the specified number of elements, each of which
  // is copy initialized from the specified value.
  inline Vector(size_t, const T&);

  // Copying.
  Vector(const Vector&);
  template <size_t otherCapacity>
  explicit Vector(const Vector<T, otherCapacity, Allocator>&);

  Vector& operator=(const Vector&);
  template <size_t otherCapacity>
  Vector& operator=(const Vector<T, otherCapacity, Allocator>&);

  // Moving.
  Vector(Vector&&);
  Vector& operator=(Vector&&);

  // Construct with an initializer list. You can do e.g.
  //     Vector<int> v({1, 2, 3});
  // or
  //     v = {4, 5, 6};
  Vector(std::initializer_list<T> elements);
  Vector& operator=(std::initializer_list<T> elements);

  // Basic inquiry about the vector's state.
  //
  // capacity() is the maximum number of elements that the Vector can hold
  // without a reallocation. It can be zero.
  size_t size() const { return m_size; }
  size_t capacity() const { return Base::capacity(); }
  bool isEmpty() const { return !size(); }

  // at() and operator[]: Obtain the reference of the element that is located
  // at the given index. The reference may be invalidated on a reallocation.
  //
  // at() can be used in cases like:
  //     pointerToVector->at(1);
  // instead of:
  //     (*pointerToVector)[1];
  T& at(size_t i) {
    RELEASE_ASSERT(i < size());
    return Base::buffer()[i];
  }
  const T& at(size_t i) const {
    RELEASE_ASSERT(i < size());
    return Base::buffer()[i];
  }

  T& operator[](size_t i) { return at(i); }
  const T& operator[](size_t i) const { return at(i); }

  // Return a pointer to the front of the backing buffer. Those pointers get
  // invalidated on a reallocation.
  T* data() { return Base::buffer(); }
  const T* data() const { return Base::buffer(); }

  // Iterators and reverse iterators. They are invalidated on a reallocation.
  iterator begin() { return data(); }
  iterator end() { return begin() + m_size; }
  const_iterator begin() const { return data(); }
  const_iterator end() const { return begin() + m_size; }

  reverse_iterator rbegin() { return reverse_iterator(end()); }
  reverse_iterator rend() { return reverse_iterator(begin()); }
  const_reverse_iterator rbegin() const {
    return const_reverse_iterator(end());
  }
  const_reverse_iterator rend() const {
    return const_reverse_iterator(begin());
  }

  // Quick access to the first and the last element. It is invalid to call
  // these functions when the vector is empty.
  T& front() { return at(0); }
  const T& front() const { return at(0); }
  T& back() { return at(size() - 1); }
  const T& back() const { return at(size() - 1); }

  // Searching.
  //
  // Comparisons are done in terms of compareElement(), which is usually
  // operator==(). find() and reverseFind() returns an index of the element
  // that is found first. If no match is found, kNotFound will be returned.
  template <typename U>
  bool contains(const U&) const;
  template <typename U>
  size_t find(const U&) const;
  template <typename U>
  size_t reverseFind(const U&) const;

  // Resize the vector to the specified size.
  //
  // These three functions are essentially similar. They differ in that
  // (1) shrink() has a DCHECK to make sure the specified size is not more than
  // size(), and (2) grow() has a DCHECK to make sure the specified size is
  // not less than size().
  //
  // When a vector shrinks, the extra elements in the back will be destructed.
  // All the iterators pointing to a to-be-destructed element will be
  // invalidated.
  //
  // When a vector grows, new elements will be added in the back, and they
  // will be default-initialized. A reallocation may happen in this case.
  void shrink(size_t);
  void grow(size_t);
  void resize(size_t);

  // Increase the capacity of the vector to at least |newCapacity|. The
  // elements in the vector are not affected. This function does not shrink
  // the size of the backing buffer, even if |newCapacity| is small. This
  // function may cause a reallocation.
  void reserveCapacity(size_t newCapacity);

  // This is similar to reserveCapacity() but must be called immediately after
  // the vector is default-constructed.
  void reserveInitialCapacity(size_t initialCapacity);

  // Shrink the backing buffer so it can contain exactly |size()| elements.
  // This function may cause a reallocation.
  void shrinkToFit() { shrinkCapacity(size()); }

  // Shrink the backing buffer if at least 50% of the vector's capacity is
  // unused. If it shrinks, the new buffer contains roughly 25% of unused
  // space. This function may cause a reallocation.
  void shrinkToReasonableCapacity() {
    if (size() * 2 < capacity())
      shrinkCapacity(size() + size() / 4 + 1);
  }

  // Remove all the elements. This function actually releases the backing
  // buffer, thus any iterators will get invalidated (including begin()).
  void clear() { shrinkCapacity(0); }

  // Insertion to the back. All of these functions except uncheckedAppend() may
  // cause a reallocation.
  //
  // push_back(value)
  //     Insert a single element to the back.
  // emplace_back(args...)
  //     Insert a single element constructed as T(args...) to the back. The
  //     element is constructed directly on the backing buffer with placement
  //     new.
  // append(buffer, size)
  // appendVector(vector)
  // appendRange(begin, end)
  //     Insert multiple elements represented by (1) |buffer| and |size|
  //     (for append), (2) |vector| (for appendVector), or (3) a pair of
  //     iterators (for appendRange) to the back. The elements will be copied.
  // uncheckedAppend(value)
  //     Insert a single element like push_back(), but this function assumes
  //     the vector has enough capacity such that it can store the new element
  //     without a reallocation. Using this function could improve the
  //     performance when you append many elements repeatedly.
  template <typename U>
  void push_back(U&&);
  template <typename... Args>
  T& emplace_back(Args&&...);
  template <typename U>
  void append(const U*, size_t);
  template <typename U, size_t otherCapacity, typename V>
  void appendVector(const Vector<U, otherCapacity, V>&);
  template <typename Iterator>
  void appendRange(Iterator begin, Iterator end);
  template <typename U>
  void uncheckedAppend(U&&);

  // Insertion to an arbitrary position. All of these functions will take
  // O(size())-time. All of the elements after |position| will be moved to
  // the new locations. |position| must be no more than size(). All of these
  // functions may cause a reallocation. In any case, all the iterators
  // pointing to an element after |position| will be invalidated.
  //
  // insert(position, value)
  //     Insert a single element at |position|.
  // insert(position, buffer, size)
  // insert(position, vector)
  //     Insert multiple elements represented by either |buffer| and |size|
  //     or |vector| at |position|. The elements will be copied.
  //
  // TODO(yutak): Why not insertVector()?
  template <typename U>
  void insert(size_t position, U&&);
  template <typename U>
  void insert(size_t position, const U*, size_t);
  template <typename U, size_t otherCapacity, typename OtherAllocator>
  void insert(size_t position, const Vector<U, otherCapacity, OtherAllocator>&);

  // Insertion to the front. All of these functions will take O(size())-time.
  // All of the elements in the vector will be moved to the new locations.
  // All of these functions may cause a reallocation. In any case, all the
  // iterators pointing to any element in the vector will be invalidated.
  //
  // prepend(value)
  //     Insert a single element to the front.
  // prepend(buffer, size)
  // prependVector(vector)
  //     Insert multiple elements represented by either |buffer| and |size| or
  //     |vector| to the front. The elements will be copied.
  template <typename U>
  void prepend(U&&);
  template <typename U>
  void prepend(const U*, size_t);
  template <typename U, size_t otherCapacity, typename OtherAllocator>
  void prependVector(const Vector<U, otherCapacity, OtherAllocator>&);

  // Remove an element or elements at the specified position. These functions
  // take O(size())-time. All of the elements after the removed ones will be
  // moved to the new locations. All the iterators pointing to any element
  // after |position| will be invalidated.
  void remove(size_t position);
  void remove(size_t position, size_t length);

  // Remove the last element. Unlike remove(), (1) this function is fast, and
  // (2) only iterators pointing to the last element will be invalidated. Other
  // references will remain valid.
  void pop_back() {
    DCHECK(!isEmpty());
    shrink(size() - 1);
  }

  // Filling the vector with the same value. If the vector has shrinked or
  // growed as a result of this call, those events may invalidate some
  // iterators. See comments for shrink() and grow().
  //
  // fill(value, size) will resize the Vector to |size|, and then copy-assign
  // or copy-initialize all the elements.
  //
  // fill(value) is a synonym for fill(value, size()).
  void fill(const T&, size_t);
  void fill(const T& val) { fill(val, size()); }

  // Swap two vectors quickly.
  void swap(Vector& other) {
    Base::swapVectorBuffer(other, OffsetRange(), OffsetRange());
  }

  // Reverse the contents.
  void reverse();

  // Maximum element count supported; allocating a vector
  // buffer with a larger count will fail.
  static size_t maxCapacity() {
    return Allocator::template maxElementCountInBackingStore<T>();
  }

  // Off-GC-heap vectors: Destructor should be called.
  // On-GC-heap vectors: Destructor should be called for inline buffers (if
  // any) but destructor shouldn't be called for vector backing since it is
  // managed by the traced GC heap.
  void finalize() {
    if (!INLINE_CAPACITY) {
      if (LIKELY(!Base::buffer()))
        return;
    }
    ANNOTATE_DELETE_BUFFER(begin(), capacity(), m_size);
    if (LIKELY(m_size) &&
        !(Allocator::isGarbageCollected && this->hasOutOfLineBuffer())) {
      TypeOperations::destruct(begin(), end());
      m_size = 0;  // Partial protection against use-after-free.
    }

    Base::destruct();
  }

  void finalizeGarbageCollectedObject() { finalize(); }

  template <typename VisitorDispatcher>
  void trace(VisitorDispatcher);

  class GCForbiddenScope {
    STACK_ALLOCATED();

   public:
    GCForbiddenScope() { Allocator::enterGCForbiddenScope(); }
    ~GCForbiddenScope() { Allocator::leaveGCForbiddenScope(); }
  };

 protected:
  using Base::checkUnusedSlots;
  using Base::clearUnusedSlots;

 private:
  void expandCapacity(size_t newMinCapacity);
  T* expandCapacity(size_t newMinCapacity, T*);
  T* expandCapacity(size_t newMinCapacity, const T* data) {
    return expandCapacity(newMinCapacity, const_cast<T*>(data));
  }

  template <typename U>
  U* expandCapacity(size_t newMinCapacity, U*);
  void shrinkCapacity(size_t newCapacity);
  template <typename U>
  void appendSlowCase(U&&);

  using Base::m_size;
  using Base::buffer;
  using Base::swapVectorBuffer;
  using Base::allocateBuffer;
  using Base::allocationSize;
};

//
// Vector out-of-line implementation
//

template <typename T, size_t inlineCapacity, typename Allocator>
inline Vector<T, inlineCapacity, Allocator>::Vector() {
  static_assert(!std::is_polymorphic<T>::value ||
                    !VectorTraits<T>::canInitializeWithMemset,
                "Cannot initialize with memset if there is a vtable");
  static_assert(Allocator::isGarbageCollected ||
                    !AllowsOnlyPlacementNew<T>::value || !IsTraceable<T>::value,
                "Cannot put DISALLOW_NEW_EXCEPT_PLACEMENT_NEW objects that "
                "have trace methods into an off-heap Vector");
  static_assert(Allocator::isGarbageCollected ||
                    !IsPointerToGarbageCollectedType<T>::value,
                "Cannot put raw pointers to garbage-collected classes into "
                "an off-heap Vector.  Use HeapVector<Member<T>> instead.");

  ANNOTATE_NEW_BUFFER(begin(), capacity(), 0);
  m_size = 0;
}

template <typename T, size_t inlineCapacity, typename Allocator>
inline Vector<T, inlineCapacity, Allocator>::Vector(size_t size) : Base(size) {
  static_assert(!std::is_polymorphic<T>::value ||
                    !VectorTraits<T>::canInitializeWithMemset,
                "Cannot initialize with memset if there is a vtable");
  static_assert(Allocator::isGarbageCollected ||
                    !AllowsOnlyPlacementNew<T>::value || !IsTraceable<T>::value,
                "Cannot put DISALLOW_NEW_EXCEPT_PLACEMENT_NEW objects that "
                "have trace methods into an off-heap Vector");
  static_assert(Allocator::isGarbageCollected ||
                    !IsPointerToGarbageCollectedType<T>::value,
                "Cannot put raw pointers to garbage-collected classes into "
                "an off-heap Vector.  Use HeapVector<Member<T>> instead.");

  ANNOTATE_NEW_BUFFER(begin(), capacity(), size);
  m_size = size;
  TypeOperations::initialize(begin(), end());
}

template <typename T, size_t inlineCapacity, typename Allocator>
inline Vector<T, inlineCapacity, Allocator>::Vector(size_t size, const T& val)
    : Base(size) {
  // TODO(yutak): Introduce these assertions. Some use sites call this function
  // in the context where T is an incomplete type.
  //
  // static_assert(!std::is_polymorphic<T>::value ||
  //               !VectorTraits<T>::canInitializeWithMemset,
  //               "Cannot initialize with memset if there is a vtable");
  // static_assert(Allocator::isGarbageCollected ||
  //               !AllowsOnlyPlacementNew<T>::value ||
  //               !IsTraceable<T>::value,
  //               "Cannot put DISALLOW_NEW_EXCEPT_PLACEMENT_NEW objects that "
  //               "have trace methods into an off-heap Vector");
  // static_assert(Allocator::isGarbageCollected ||
  //               !IsPointerToGarbageCollectedType<T>::value,
  //               "Cannot put raw pointers to garbage-collected classes into "
  //               "an off-heap Vector.  Use HeapVector<Member<T>> instead.");

  ANNOTATE_NEW_BUFFER(begin(), capacity(), size);
  m_size = size;
  TypeOperations::uninitializedFill(begin(), end(), val);
}

template <typename T, size_t inlineCapacity, typename Allocator>
Vector<T, inlineCapacity, Allocator>::Vector(const Vector& other)
    : Base(other.capacity()) {
  ANNOTATE_NEW_BUFFER(begin(), capacity(), other.size());
  m_size = other.size();
  TypeOperations::uninitializedCopy(other.begin(), other.end(), begin());
}

template <typename T, size_t inlineCapacity, typename Allocator>
template <size_t otherCapacity>
Vector<T, inlineCapacity, Allocator>::Vector(
    const Vector<T, otherCapacity, Allocator>& other)
    : Base(other.capacity()) {
  ANNOTATE_NEW_BUFFER(begin(), capacity(), other.size());
  m_size = other.size();
  TypeOperations::uninitializedCopy(other.begin(), other.end(), begin());
}

template <typename T, size_t inlineCapacity, typename Allocator>
Vector<T, inlineCapacity, Allocator>& Vector<T, inlineCapacity, Allocator>::
operator=(const Vector<T, inlineCapacity, Allocator>& other) {
  if (UNLIKELY(&other == this))
    return *this;

  if (size() > other.size()) {
    shrink(other.size());
  } else if (other.size() > capacity()) {
    clear();
    reserveCapacity(other.size());
    DCHECK(begin());
  }

  ANNOTATE_CHANGE_SIZE(begin(), capacity(), m_size, other.size());
  std::copy(other.begin(), other.begin() + size(), begin());
  TypeOperations::uninitializedCopy(other.begin() + size(), other.end(), end());
  m_size = other.size();

  return *this;
}

inline bool typelessPointersAreEqual(const void* a, const void* b) {
  return a == b;
}

template <typename T, size_t inlineCapacity, typename Allocator>
template <size_t otherCapacity>
Vector<T, inlineCapacity, Allocator>& Vector<T, inlineCapacity, Allocator>::
operator=(const Vector<T, otherCapacity, Allocator>& other) {
  // If the inline capacities match, we should call the more specific
  // template.  If the inline capacities don't match, the two objects
  // shouldn't be allocated the same address.
  DCHECK(!typelessPointersAreEqual(&other, this));

  if (size() > other.size()) {
    shrink(other.size());
  } else if (other.size() > capacity()) {
    clear();
    reserveCapacity(other.size());
    DCHECK(begin());
  }

  ANNOTATE_CHANGE_SIZE(begin(), capacity(), m_size, other.size());
  std::copy(other.begin(), other.begin() + size(), begin());
  TypeOperations::uninitializedCopy(other.begin() + size(), other.end(), end());
  m_size = other.size();

  return *this;
}

template <typename T, size_t inlineCapacity, typename Allocator>
Vector<T, inlineCapacity, Allocator>::Vector(
    Vector<T, inlineCapacity, Allocator>&& other) {
  m_size = 0;
  // It's a little weird to implement a move constructor using swap but this
  // way we don't have to add a move constructor to VectorBuffer.
  swap(other);
}

template <typename T, size_t inlineCapacity, typename Allocator>
Vector<T, inlineCapacity, Allocator>& Vector<T, inlineCapacity, Allocator>::
operator=(Vector<T, inlineCapacity, Allocator>&& other) {
  swap(other);
  return *this;
}

template <typename T, size_t inlineCapacity, typename Allocator>
Vector<T, inlineCapacity, Allocator>::Vector(std::initializer_list<T> elements)
    : Base(elements.size()) {
  ANNOTATE_NEW_BUFFER(begin(), capacity(), elements.size());
  m_size = elements.size();
  TypeOperations::uninitializedCopy(elements.begin(), elements.end(), begin());
}

template <typename T, size_t inlineCapacity, typename Allocator>
Vector<T, inlineCapacity, Allocator>& Vector<T, inlineCapacity, Allocator>::
operator=(std::initializer_list<T> elements) {
  if (size() > elements.size()) {
    shrink(elements.size());
  } else if (elements.size() > capacity()) {
    clear();
    reserveCapacity(elements.size());
    DCHECK(begin());
  }

  ANNOTATE_CHANGE_SIZE(begin(), capacity(), m_size, elements.size());
  std::copy(elements.begin(), elements.begin() + m_size, begin());
  TypeOperations::uninitializedCopy(elements.begin() + m_size, elements.end(),
                                    end());
  m_size = elements.size();

  return *this;
}

template <typename T, size_t inlineCapacity, typename Allocator>
template <typename U>
bool Vector<T, inlineCapacity, Allocator>::contains(const U& value) const {
  return find(value) != kNotFound;
}

template <typename T, size_t inlineCapacity, typename Allocator>
template <typename U>
size_t Vector<T, inlineCapacity, Allocator>::find(const U& value) const {
  const T* b = begin();
  const T* e = end();
  for (const T* iter = b; iter < e; ++iter) {
    if (TypeOperations::compareElement(*iter, value))
      return iter - b;
  }
  return kNotFound;
}

template <typename T, size_t inlineCapacity, typename Allocator>
template <typename U>
size_t Vector<T, inlineCapacity, Allocator>::reverseFind(const U& value) const {
  const T* b = begin();
  const T* iter = end();
  while (iter > b) {
    --iter;
    if (TypeOperations::compareElement(*iter, value))
      return iter - b;
  }
  return kNotFound;
}

template <typename T, size_t inlineCapacity, typename Allocator>
void Vector<T, inlineCapacity, Allocator>::fill(const T& val, size_t newSize) {
  if (size() > newSize) {
    shrink(newSize);
  } else if (newSize > capacity()) {
    clear();
    reserveCapacity(newSize);
    DCHECK(begin());
  }

  ANNOTATE_CHANGE_SIZE(begin(), capacity(), m_size, newSize);
  std::fill(begin(), end(), val);
  TypeOperations::uninitializedFill(end(), begin() + newSize, val);
  m_size = newSize;
}

template <typename T, size_t inlineCapacity, typename Allocator>
void Vector<T, inlineCapacity, Allocator>::expandCapacity(
    size_t newMinCapacity) {
  size_t oldCapacity = capacity();
  size_t expandedCapacity = oldCapacity;
  // We use a more aggressive expansion strategy for Vectors with inline
  // storage.  This is because they are more likely to be on the stack, so the
  // risk of heap bloat is minimized.  Furthermore, exceeding the inline
  // capacity limit is not supposed to happen in the common case and may
  // indicate a pathological condition or microbenchmark.
  if (INLINE_CAPACITY) {
    expandedCapacity *= 2;
    // Check for integer overflow, which could happen in the 32-bit build.
    RELEASE_ASSERT(expandedCapacity > oldCapacity);
  } else {
    // This cannot integer overflow.
    // On 64-bit, the "expanded" integer is 32-bit, and any encroachment
    // above 2^32 will fail allocation in allocateBuffer().  On 32-bit,
    // there's not enough address space to hold the old and new buffers.  In
    // addition, our underlying allocator is supposed to always fail on >
    // (2^31 - 1) allocations.
    expandedCapacity += (expandedCapacity / 4) + 1;
  }
  reserveCapacity(std::max(
      newMinCapacity,
      std::max(static_cast<size_t>(kInitialVectorSize), expandedCapacity)));
}

template <typename T, size_t inlineCapacity, typename Allocator>
T* Vector<T, inlineCapacity, Allocator>::expandCapacity(size_t newMinCapacity,
                                                        T* ptr) {
  if (ptr < begin() || ptr >= end()) {
    expandCapacity(newMinCapacity);
    return ptr;
  }
  size_t index = ptr - begin();
  expandCapacity(newMinCapacity);
  return begin() + index;
}

template <typename T, size_t inlineCapacity, typename Allocator>
template <typename U>
inline U* Vector<T, inlineCapacity, Allocator>::expandCapacity(
    size_t newMinCapacity,
    U* ptr) {
  expandCapacity(newMinCapacity);
  return ptr;
}

template <typename T, size_t inlineCapacity, typename Allocator>
inline void Vector<T, inlineCapacity, Allocator>::resize(size_t size) {
  if (size <= m_size) {
    TypeOperations::destruct(begin() + size, end());
    clearUnusedSlots(begin() + size, end());
    ANNOTATE_CHANGE_SIZE(begin(), capacity(), m_size, size);
  } else {
    if (size > capacity())
      expandCapacity(size);
    ANNOTATE_CHANGE_SIZE(begin(), capacity(), m_size, size);
    TypeOperations::initialize(end(), begin() + size);
  }

  m_size = size;
}

template <typename T, size_t inlineCapacity, typename Allocator>
void Vector<T, inlineCapacity, Allocator>::shrink(size_t size) {
  DCHECK_LE(size, m_size);
  TypeOperations::destruct(begin() + size, end());
  clearUnusedSlots(begin() + size, end());
  ANNOTATE_CHANGE_SIZE(begin(), capacity(), m_size, size);
  m_size = size;
}

template <typename T, size_t inlineCapacity, typename Allocator>
void Vector<T, inlineCapacity, Allocator>::grow(size_t size) {
  DCHECK_GE(size, m_size);
  if (size > capacity())
    expandCapacity(size);
  ANNOTATE_CHANGE_SIZE(begin(), capacity(), m_size, size);
  TypeOperations::initialize(end(), begin() + size);
  m_size = size;
}

template <typename T, size_t inlineCapacity, typename Allocator>
void Vector<T, inlineCapacity, Allocator>::reserveCapacity(size_t newCapacity) {
  if (UNLIKELY(newCapacity <= capacity()))
    return;
  T* oldBuffer = begin();
  if (!oldBuffer) {
    Base::allocateBuffer(newCapacity);
    return;
  }
#ifdef ANNOTATE_CONTIGUOUS_CONTAINER
  size_t oldCapacity = capacity();
#endif
  // The Allocator::isGarbageCollected check is not needed.  The check is just
  // a static hint for a compiler to indicate that Base::expandBuffer returns
  // false if Allocator is a PartitionAllocator.
  if (Allocator::isGarbageCollected && Base::expandBuffer(newCapacity)) {
    ANNOTATE_CHANGE_CAPACITY(begin(), oldCapacity, m_size, capacity());
    return;
  }
  T* oldEnd = end();
  Base::allocateExpandedBuffer(newCapacity);
  ANNOTATE_NEW_BUFFER(begin(), capacity(), m_size);
  TypeOperations::move(oldBuffer, oldEnd, begin());
  clearUnusedSlots(oldBuffer, oldEnd);
  ANNOTATE_DELETE_BUFFER(oldBuffer, oldCapacity, m_size);
  Base::deallocateBuffer(oldBuffer);
}

template <typename T, size_t inlineCapacity, typename Allocator>
inline void Vector<T, inlineCapacity, Allocator>::reserveInitialCapacity(
    size_t initialCapacity) {
  DCHECK(!m_size);
  DCHECK(capacity() == INLINE_CAPACITY);
  if (initialCapacity > INLINE_CAPACITY) {
    ANNOTATE_DELETE_BUFFER(begin(), capacity(), m_size);
    Base::allocateBuffer(initialCapacity);
    ANNOTATE_NEW_BUFFER(begin(), capacity(), m_size);
  }
}

template <typename T, size_t inlineCapacity, typename Allocator>
void Vector<T, inlineCapacity, Allocator>::shrinkCapacity(size_t newCapacity) {
  if (newCapacity >= capacity())
    return;

  if (newCapacity < size())
    shrink(newCapacity);

  T* oldBuffer = begin();
#ifdef ANNOTATE_CONTIGUOUS_CONTAINER
  size_t oldCapacity = capacity();
#endif
  if (newCapacity > 0) {
    if (Base::shrinkBuffer(newCapacity)) {
      ANNOTATE_CHANGE_CAPACITY(begin(), oldCapacity, m_size, capacity());
      return;
    }

    T* oldEnd = end();
    Base::allocateBuffer(newCapacity);
    if (begin() != oldBuffer) {
      ANNOTATE_NEW_BUFFER(begin(), capacity(), m_size);
      TypeOperations::move(oldBuffer, oldEnd, begin());
      clearUnusedSlots(oldBuffer, oldEnd);
      ANNOTATE_DELETE_BUFFER(oldBuffer, oldCapacity, m_size);
    }
  } else {
    Base::resetBufferPointer();
#ifdef ANNOTATE_CONTIGUOUS_CONTAINER
    if (oldBuffer != begin()) {
      ANNOTATE_NEW_BUFFER(begin(), capacity(), m_size);
      ANNOTATE_DELETE_BUFFER(oldBuffer, oldCapacity, m_size);
    }
#endif
  }

  Base::deallocateBuffer(oldBuffer);
}

// Templatizing these is better than just letting the conversion happen
// implicitly, because for instance it allows a PassRefPtr to be appended to a
// RefPtr vector without refcount thrash.

template <typename T, size_t inlineCapacity, typename Allocator>
template <typename U>
ALWAYS_INLINE void Vector<T, inlineCapacity, Allocator>::push_back(U&& val) {
  DCHECK(Allocator::isAllocationAllowed());
  if (LIKELY(size() != capacity())) {
    ANNOTATE_CHANGE_SIZE(begin(), capacity(), m_size, m_size + 1);
    new (NotNull, end()) T(std::forward<U>(val));
    ++m_size;
    return;
  }

  appendSlowCase(std::forward<U>(val));
}

template <typename T, size_t inlineCapacity, typename Allocator>
template <typename... Args>
ALWAYS_INLINE T& Vector<T, inlineCapacity, Allocator>::emplace_back(
    Args&&... args) {
  static_assert(sizeof...(Args), "grow() must be called instead");
  static_assert(sizeof...(Args) != 1, "append() must be called instead");

  DCHECK(Allocator::isAllocationAllowed());
  if (UNLIKELY(size() == capacity()))
    expandCapacity(size() + 1);

  ANNOTATE_CHANGE_SIZE(begin(), capacity(), m_size, m_size + 1);
  T* t = new (NotNull, end()) T(std::forward<Args>(args)...);
  ++m_size;
  return *t;
}

template <typename T, size_t inlineCapacity, typename Allocator>
template <typename U>
void Vector<T, inlineCapacity, Allocator>::append(const U* data,
                                                  size_t dataSize) {
  DCHECK(Allocator::isAllocationAllowed());
  size_t newSize = m_size + dataSize;
  if (newSize > capacity()) {
    data = expandCapacity(newSize, data);
    DCHECK(begin());
  }
  RELEASE_ASSERT(newSize >= m_size);
  T* dest = end();
  ANNOTATE_CHANGE_SIZE(begin(), capacity(), m_size, newSize);
  VectorCopier<VectorTraits<T>::canCopyWithMemcpy, T>::uninitializedCopy(
      data, &data[dataSize], dest);
  m_size = newSize;
}

template <typename T, size_t inlineCapacity, typename Allocator>
template <typename U>
NEVER_INLINE void Vector<T, inlineCapacity, Allocator>::appendSlowCase(
    U&& val) {
  DCHECK_EQ(size(), capacity());

  typename std::remove_reference<U>::type* ptr = &val;
  ptr = expandCapacity(size() + 1, ptr);
  DCHECK(begin());

  ANNOTATE_CHANGE_SIZE(begin(), capacity(), m_size, m_size + 1);
  new (NotNull, end()) T(std::forward<U>(*ptr));
  ++m_size;
}

template <typename T, size_t inlineCapacity, typename Allocator>
template <typename U, size_t otherCapacity, typename OtherAllocator>
inline void Vector<T, inlineCapacity, Allocator>::appendVector(
    const Vector<U, otherCapacity, OtherAllocator>& val) {
  append(val.begin(), val.size());
}

template <typename T, size_t inlineCapacity, typename Allocator>
template <typename Iterator>
void Vector<T, inlineCapacity, Allocator>::appendRange(Iterator begin,
                                                       Iterator end) {
  for (Iterator it = begin; it != end; ++it)
    push_back(*it);
}

// This version of append saves a branch in the case where you know that the
// vector's capacity is large enough for the append to succeed.
template <typename T, size_t inlineCapacity, typename Allocator>
template <typename U>
ALWAYS_INLINE void Vector<T, inlineCapacity, Allocator>::uncheckedAppend(
    U&& val) {
#ifdef ANNOTATE_CONTIGUOUS_CONTAINER
  // Vectors in ASAN builds don't have inlineCapacity.
  push_back(std::forward<U>(val));
#else
  DCHECK_LT(size(), capacity());
  ANNOTATE_CHANGE_SIZE(begin(), capacity(), m_size, m_size + 1);
  new (NotNull, end()) T(std::forward<U>(val));
  ++m_size;
#endif
}

template <typename T, size_t inlineCapacity, typename Allocator>
template <typename U>
inline void Vector<T, inlineCapacity, Allocator>::insert(size_t position,
                                                         U&& val) {
  DCHECK(Allocator::isAllocationAllowed());
  RELEASE_ASSERT(position <= size());
  typename std::remove_reference<U>::type* data = &val;
  if (size() == capacity()) {
    data = expandCapacity(size() + 1, data);
    DCHECK(begin());
  }
  ANNOTATE_CHANGE_SIZE(begin(), capacity(), m_size, m_size + 1);
  T* spot = begin() + position;
  TypeOperations::moveOverlapping(spot, end(), spot + 1);
  new (NotNull, spot) T(std::forward<U>(*data));
  ++m_size;
}

template <typename T, size_t inlineCapacity, typename Allocator>
template <typename U>
void Vector<T, inlineCapacity, Allocator>::insert(size_t position,
                                                  const U* data,
                                                  size_t dataSize) {
  DCHECK(Allocator::isAllocationAllowed());
  RELEASE_ASSERT(position <= size());
  size_t newSize = m_size + dataSize;
  if (newSize > capacity()) {
    data = expandCapacity(newSize, data);
    DCHECK(begin());
  }
  RELEASE_ASSERT(newSize >= m_size);
  ANNOTATE_CHANGE_SIZE(begin(), capacity(), m_size, newSize);
  T* spot = begin() + position;
  TypeOperations::moveOverlapping(spot, end(), spot + dataSize);
  VectorCopier<VectorTraits<T>::canCopyWithMemcpy, T>::uninitializedCopy(
      data, &data[dataSize], spot);
  m_size = newSize;
}

template <typename T, size_t inlineCapacity, typename Allocator>
template <typename U, size_t otherCapacity, typename OtherAllocator>
inline void Vector<T, inlineCapacity, Allocator>::insert(
    size_t position,
    const Vector<U, otherCapacity, OtherAllocator>& val) {
  insert(position, val.begin(), val.size());
}

template <typename T, size_t inlineCapacity, typename Allocator>
template <typename U>
inline void Vector<T, inlineCapacity, Allocator>::prepend(U&& val) {
  insert(0, std::forward<U>(val));
}

template <typename T, size_t inlineCapacity, typename Allocator>
template <typename U>
void Vector<T, inlineCapacity, Allocator>::prepend(const U* data,
                                                   size_t dataSize) {
  insert(0, data, dataSize);
}

template <typename T, size_t inlineCapacity, typename Allocator>
template <typename U, size_t otherCapacity, typename OtherAllocator>
inline void Vector<T, inlineCapacity, Allocator>::prependVector(
    const Vector<U, otherCapacity, OtherAllocator>& val) {
  insert(0, val.begin(), val.size());
}

template <typename T, size_t inlineCapacity, typename Allocator>
inline void Vector<T, inlineCapacity, Allocator>::remove(size_t position) {
  RELEASE_ASSERT(position < size());
  T* spot = begin() + position;
  spot->~T();
  TypeOperations::moveOverlapping(spot + 1, end(), spot);
  clearUnusedSlots(end() - 1, end());
  ANNOTATE_CHANGE_SIZE(begin(), capacity(), m_size, m_size - 1);
  --m_size;
}

template <typename T, size_t inlineCapacity, typename Allocator>
inline void Vector<T, inlineCapacity, Allocator>::remove(size_t position,
                                                         size_t length) {
  SECURITY_DCHECK(position <= size());
  if (!length)
    return;
  RELEASE_ASSERT(position + length <= size());
  T* beginSpot = begin() + position;
  T* endSpot = beginSpot + length;
  TypeOperations::destruct(beginSpot, endSpot);
  TypeOperations::moveOverlapping(endSpot, end(), beginSpot);
  clearUnusedSlots(end() - length, end());
  ANNOTATE_CHANGE_SIZE(begin(), capacity(), m_size, m_size - length);
  m_size -= length;
}

template <typename T, size_t inlineCapacity, typename Allocator>
inline void Vector<T, inlineCapacity, Allocator>::reverse() {
  for (size_t i = 0; i < m_size / 2; ++i)
    std::swap(at(i), at(m_size - 1 - i));
}

template <typename T, size_t inlineCapacity, typename Allocator>
inline void swap(Vector<T, inlineCapacity, Allocator>& a,
                 Vector<T, inlineCapacity, Allocator>& b) {
  a.swap(b);
}

template <typename T,
          size_t inlineCapacityA,
          size_t inlineCapacityB,
          typename Allocator>
bool operator==(const Vector<T, inlineCapacityA, Allocator>& a,
                const Vector<T, inlineCapacityB, Allocator>& b) {
  if (a.size() != b.size())
    return false;
  if (a.isEmpty())
    return true;
  return VectorTypeOperations<T>::compare(a.data(), b.data(), a.size());
}

template <typename T,
          size_t inlineCapacityA,
          size_t inlineCapacityB,
          typename Allocator>
inline bool operator!=(const Vector<T, inlineCapacityA, Allocator>& a,
                       const Vector<T, inlineCapacityB, Allocator>& b) {
  return !(a == b);
}

// This is only called if the allocator is a HeapAllocator. It is used when
// visiting during a tracing GC.
template <typename T, size_t inlineCapacity, typename Allocator>
template <typename VisitorDispatcher>
void Vector<T, inlineCapacity, Allocator>::trace(VisitorDispatcher visitor) {
  DCHECK(Allocator::isGarbageCollected) << "Garbage collector must be enabled.";
  if (!buffer())
    return;
  if (this->hasOutOfLineBuffer()) {
    // This is a performance optimization for a case where the buffer has
    // been already traced by somewhere. This can happen if the conservative
    // scanning traced an on-stack (false-positive or real) pointer to the
    // HeapVector, and then visitor->trace() traces the HeapVector.
    if (Allocator::isHeapObjectAlive(buffer()))
      return;
    Allocator::markNoTracing(visitor, buffer());
    Allocator::registerBackingStoreReference(visitor, Base::bufferSlot());
  }
  const T* bufferBegin = buffer();
  const T* bufferEnd = buffer() + size();
  if (IsTraceableInCollectionTrait<VectorTraits<T>>::value) {
    for (const T* bufferEntry = bufferBegin; bufferEntry != bufferEnd;
         bufferEntry++)
      Allocator::template trace<VisitorDispatcher, T, VectorTraits<T>>(
          visitor, *const_cast<T*>(bufferEntry));
    checkUnusedSlots(buffer() + size(), buffer() + capacity());
  }
}

}  // namespace WTF

using WTF::Vector;

#endif  // WTF_Vector_h

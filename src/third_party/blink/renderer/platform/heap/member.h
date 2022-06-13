// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_MEMBER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_MEMBER_H_

#include "third_party/blink/renderer/platform/heap/thread_state_storage.h"
#include "third_party/blink/renderer/platform/heap/write_barrier.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/construct_traits.h"
#include "third_party/blink/renderer/platform/wtf/hash_functions.h"
#include "third_party/blink/renderer/platform/wtf/hash_traits.h"
#include "third_party/blink/renderer/platform/wtf/type_traits.h"
#include "v8/include/cppgc/member.h"

namespace blink {

template <typename T>
using Member = cppgc::Member<T>;

template <typename T>
using WeakMember = cppgc::WeakMember<T>;

template <typename T>
using UntracedMember = cppgc::UntracedMember<T>;

template <typename T>
inline bool IsHashTableDeletedValue(const Member<T>& m) {
  return m == cppgc::kSentinelPointer;
}

constexpr auto kMemberDeletedValue = cppgc::kSentinelPointer;

template <typename T>
struct ThreadingTrait<blink::Member<T>> {
  STATIC_ONLY(ThreadingTrait);
  static constexpr ThreadAffinity kAffinity = ThreadingTrait<T>::kAffinity;
};

template <typename T>
struct ThreadingTrait<blink::WeakMember<T>> {
  STATIC_ONLY(ThreadingTrait);
  static constexpr ThreadAffinity kAffinity = ThreadingTrait<T>::kAffinity;
};

template <typename T>
struct ThreadingTrait<blink::UntracedMember<T>> {
  STATIC_ONLY(ThreadingTrait);
  static constexpr ThreadAffinity kAffinity = ThreadingTrait<T>::kAffinity;
};

template <typename T>
inline void swap(Member<T>& a, Member<T>& b) {
  a.Swap(b);
}

static constexpr bool kBlinkMemberGCHasDebugChecks =
    !std::is_same<cppgc::internal::DefaultMemberCheckingPolicy,
                  cppgc::internal::DisabledCheckingPolicy>::value;

}  // namespace blink

namespace WTF {

// Default hash for hash tables with Member<>-derived elements.
template <typename T>
struct MemberHash : PtrHash<T> {
  STATIC_ONLY(MemberHash);
  template <typename U>
  static unsigned GetHash(const U& key) {
    return PtrHash<T>::GetHash(key);
  }
  template <typename U, typename V>
  static bool Equal(const U& a, const V& b) {
    return a == b;
  }
};

template <typename T>
struct DefaultHash<blink::Member<T>> {
  STATIC_ONLY(DefaultHash);
  using Hash = MemberHash<T>;
};

template <typename T>
struct DefaultHash<blink::WeakMember<T>> {
  STATIC_ONLY(DefaultHash);
  using Hash = MemberHash<T>;
};

template <typename T>
struct DefaultHash<blink::UntracedMember<T>> {
  STATIC_ONLY(DefaultHash);
  using Hash = MemberHash<T>;
};

template <typename T>
struct IsTraceable<blink::Member<T>> {
  STATIC_ONLY(IsTraceable);
  static const bool value = true;
};

template <typename T>
struct IsWeak<blink::WeakMember<T>> : std::true_type {};

template <typename T>
struct IsTraceable<blink::WeakMember<T>> {
  STATIC_ONLY(IsTraceable);
  static const bool value = true;
};

template <typename T, typename MemberType>
struct BaseMemberHashTraits : SimpleClassHashTraits<MemberType> {
  STATIC_ONLY(BaseMemberHashTraits);

  using PeekInType = T*;
  using PeekOutType = T*;
  using IteratorGetType = MemberType*;
  using IteratorConstGetType = const MemberType*;
  using IteratorReferenceType = MemberType&;
  using IteratorConstReferenceType = const MemberType&;

  static PeekOutType Peek(const MemberType& value) { return value; }

  static IteratorReferenceType GetToReferenceConversion(IteratorGetType x) {
    return *x;
  }

  static IteratorConstReferenceType GetToReferenceConstConversion(
      IteratorConstGetType x) {
    return *x;
  }

  template <typename U>
  static void Store(const U& value, MemberType& storage) {
    storage = value;
  }

  static void ConstructDeletedValue(MemberType& slot, bool) {
    slot = cppgc::kSentinelPointer;
  }

  static bool IsDeletedValue(const MemberType& value) {
    return value.Get() == cppgc::kSentinelPointer;
  }
};

template <typename T>
struct HashTraits<blink::Member<T>>
    : BaseMemberHashTraits<T, blink::Member<T>> {
  static constexpr bool kCanTraceConcurrently = true;
};

template <typename T>
struct HashTraits<blink::WeakMember<T>>
    : BaseMemberHashTraits<T, blink::WeakMember<T>> {
  static constexpr bool kCanTraceConcurrently = true;
};

template <typename T>
struct HashTraits<blink::UntracedMember<T>>
    : BaseMemberHashTraits<T, blink::UntracedMember<T>> {};

template <typename T, typename Traits, typename Allocator>
class MemberConstructTraits {
  STATIC_ONLY(MemberConstructTraits);

 public:
  template <typename... Args>
  static T* Construct(void* location, Args&&... args) {
    return new (NotNullTag::kNotNull, location) T(std::forward<Args>(args)...);
  }

  static void NotifyNewElement(T* element) {
    blink::WriteBarrier::DispatchForObject(element);
  }

  template <typename... Args>
  static T* ConstructAndNotifyElement(void* location, Args&&... args) {
    // ConstructAndNotifyElement updates an existing Member which might
    // also be comncurrently traced while we update it. The regular ctors
    // for Member don't use an atomic write which can lead to data races.
    T* object = Construct(location, std::forward<Args>(args)...,
                          typename T::AtomicInitializerTag());
    NotifyNewElement(object);
    return object;
  }

  static void NotifyNewElements(T* array, size_t len) {
    while (len-- > 0) {
      blink::WriteBarrier::DispatchForObject(array);
      array++;
    }
  }
};

template <typename T, typename Traits, typename Allocator>
class ConstructTraits<blink::Member<T>, Traits, Allocator>
    : public MemberConstructTraits<blink::Member<T>, Traits, Allocator> {};

template <typename T, typename Traits, typename Allocator>
class ConstructTraits<blink::WeakMember<T>, Traits, Allocator>
    : public MemberConstructTraits<blink::WeakMember<T>, Traits, Allocator> {};

}  // namespace WTF

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_MEMBER_H_

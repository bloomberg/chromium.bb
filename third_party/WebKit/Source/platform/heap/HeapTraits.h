// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef HeapTraits_h
#define HeapTraits_h

#include <type_traits>
#include "platform/heap/HeapAllocator.h"
#include "platform/heap/Member.h"
#include "platform/wtf/TypeTraits.h"

namespace blink {

// Given a type T, sets |type| to either Member<T> or just T depending on
// whether T is a garbage-collected type.
template <typename T>
struct AddMemberIfNeeded final {
  using type = typename std::
      conditional<WTF::IsGarbageCollectedType<T>::value, Member<T>, T>::type;
};

// Given a type T, sets |type| to a HeapVector<T>, HeapVector<Member<T>> or
// Vector<T> depending on T.
template <typename T>
struct VectorOf final {
  using type =
      typename std::conditional<WTF::IsTraceable<T>::value,
                                HeapVector<typename AddMemberIfNeeded<T>::type>,
                                Vector<T>>::type;
};

// Given types T and U, sets |type| to one of the following:
// - HeapVector<std::pair<V, X>>
//   (where V is either T or Member<T> and X is either U or Member<U>)
// - Vector<std::pair<T, U>>
template <typename T, typename U>
struct VectorOfPairs final {
  using type = typename std::conditional<
      WTF::IsTraceable<T>::value || WTF::IsTraceable<U>::value,
      HeapVector<std::pair<typename AddMemberIfNeeded<T>::type,
                           typename AddMemberIfNeeded<U>::type>>,
      Vector<std::pair<T, U>>>::type;
};

}  // namespace blink

#endif  // HeapTraits_h

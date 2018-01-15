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

// Given a type T, returns a type that is either Member<T> or just T depending
// on whether T is a garbage-collected type.
template <typename T>
using AddMemberIfNeeded =
    std::conditional_t<WTF::IsGarbageCollectedType<T>::value, Member<T>, T>;

// Given a type T, returns a type that is either HeapVector<T>,
// HeapVector<Member<T>> or Vector<T> depending on T.
template <typename T>
using VectorOf = std::conditional_t<WTF::IsTraceable<T>::value,
                                    HeapVector<AddMemberIfNeeded<T>>,
                                    Vector<T>>;

// Given types T and U, returns a type that is one of the following:
// - HeapVector<std::pair<V, X>>
//   (where V is either T or Member<T> and X is either U or Member<U>)
// - Vector<std::pair<T, U>>
template <typename T, typename U>
using VectorOfPairs = std::conditional_t<
    WTF::IsTraceable<T>::value || WTF::IsTraceable<U>::value,
    HeapVector<std::pair<AddMemberIfNeeded<T>, AddMemberIfNeeded<U>>>,
    Vector<std::pair<T, U>>>;

}  // namespace blink

#endif  // HeapTraits_h

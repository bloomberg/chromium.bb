// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef Optional_h
#define Optional_h

#include "base/optional.h"
#include "platform/wtf/TypeTraits.h"

namespace WTF {

// WTF::Optional is base::Optional. See base/optional.h for documentation.
//
// A clang plugin enforces that garbage collected types are not allocated
// outside of the heap, similarly we enforce that one doesn't create garbage
// collected types nested inside an Optional.
template <typename T>
using Optional =
    typename std::enable_if<!IsGarbageCollectedType<T>::value ||
                                IsPersistentReferenceType<T>::value,
                            base::Optional<T>>::type;

constexpr base::nullopt_t nullopt = base::nullopt;
constexpr base::in_place_t in_place = base::in_place;

template <typename T>
constexpr Optional<typename std::decay<T>::type> make_optional(T&& value) {
  return base::make_optional(std::forward<T>(value));
}

}  // namespace WTF

using WTF::Optional;

#endif  // Optional_h

// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_WTF_OPTIONAL_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_WTF_OPTIONAL_H_

#include "base/optional.h"
#include "base/stl_util.h"

namespace WTF {

// WTF::Optional is base::Optional. See base/optional.h for documentation.
//
// A clang plugin enforces that garbage collected types are not allocated
// outside of the heap. GC containers such as HeapVector are allowed though.
template <typename T>
using Optional = base::Optional<T>;

constexpr base::nullopt_t nullopt = base::nullopt;
constexpr base::in_place_t in_place = base::in_place;

template <typename T>
constexpr Optional<typename std::decay<T>::type> make_optional(T&& value) {
  return base::make_optional(std::forward<T>(value));
}

template <typename T>
T* OptionalOrNullptr(Optional<T>& optional) {
  return base::OptionalOrNullptr(optional);
}

}  // namespace WTF

using WTF::Optional;

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_WTF_OPTIONAL_H_

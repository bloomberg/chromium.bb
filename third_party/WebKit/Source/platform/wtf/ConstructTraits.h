// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WTF_ConstructTraits_h
#define WTF_ConstructTraits_h

#include "platform/wtf/Allocator.h"
#include "platform/wtf/TypeTraits.h"
#include "platform/wtf/VectorTraits.h"

namespace WTF {

// ConstructTraits is used to construct elements in WTF collections. All
// in-place constructions that may assign Oilpan objects must be dispatched
// through ConstructAndNotifyElement.
template <typename T, typename Traits, typename Allocator>
class ConstructTraits {
  STATIC_ONLY(ConstructTraits);

 public:
  // Construct a single element that would otherwise be constructed using
  // placement new.
  template <typename... Args>
  static T* ConstructAndNotifyElement(void* location, Args&&... args) {
    T* object = new (NotNull, location) T(std::forward<Args>(args)...);
    Allocator::template NotifyNewObject<T, Traits>(object);
    return object;
  }

  // After constructing elements using memcopy or memmove (or similar)
  // |NotifyNewElements| needs to be called to propagate that information.
  static void NotifyNewElements(T* array, size_t len) {
    Allocator::template NotifyNewObjects<T, Traits>(array, len);
  }
};

}  // namespace WTF

#endif  // WTF_ConstructTraits_h

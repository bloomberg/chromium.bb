// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PtrUtil_h
#define PtrUtil_h

#include "base/memory/ptr_util.h"
#include "platform/wtf/TypeTraits.h"

#include <memory>
#include <type_traits>

namespace WTF {

template <typename T>
std::unique_ptr<T> WrapUnique(T* ptr) {
  static_assert(
      !WTF::IsGarbageCollectedType<T>::value,
      "Garbage collected types should not be stored in std::unique_ptr!");
  return std::unique_ptr<T>(ptr);
}

}  // namespace WTF

#endif  // PtrUtil_h

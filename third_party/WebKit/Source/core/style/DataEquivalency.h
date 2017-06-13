// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DataEquivalency_h
#define DataEquivalency_h

#include <memory>
#include "platform/wtf/RefPtr.h"

namespace blink {

template <typename T>
class Persistent;
template <typename T>
class Member;
template <typename T>
class DataRef;
template <typename T>
class DataPersistent;

template <typename T>
bool DataEquivalent(const T* a, const T* b) {
  if (a == b)
    return true;
  if (!a || !b)
    return false;
  return *a == *b;
}

template <typename T>
bool DataEquivalent(const RefPtr<T>& a, const RefPtr<T>& b) {
  return DataEquivalent(a.Get(), b.Get());
}

template <typename T>
bool DataEquivalent(const Persistent<T>& a, const Persistent<T>& b) {
  return DataEquivalent(a.Get(), b.Get());
}

template <typename T>
bool DataEquivalent(const Member<T>& a, const Member<T>& b) {
  return DataEquivalent(a.Get(), b.Get());
}

template <typename T>
bool DataEquivalent(const std::unique_ptr<T>& a, const std::unique_ptr<T>& b) {
  return DataEquivalent(a.get(), b.get());
}

// TODO(shend): Remove this once all subgroups of StyleRareNonInheritedData are
// generated
template <typename T>
bool DataEquivalent(const DataRef<T>& a, const DataRef<T>& b) {
  return DataEquivalent(a.Get(), b.Get());
}

template <typename T>
bool DataEquivalent(const DataPersistent<T>& a, const DataPersistent<T>& b) {
  return DataEquivalent(a.Get(), b.Get());
}

}  // namespace blink

#endif  // DataEquivalency_h

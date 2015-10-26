// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_LIB_VALUE_TRAITS_H_
#define MOJO_PUBLIC_CPP_BINDINGS_LIB_VALUE_TRAITS_H_

#include "mojo/public/cpp/bindings/lib/template_util.h"

namespace mojo {

template <typename T>
class Array;

template <typename T>
class AssociatedInterfacePtrInfo;

template <typename T>
class AssociatedInterfaceRequest;

template <typename T>
class InlinedStructPtr;

template <typename T>
class InterfacePtr;

template <typename T>
class InterfaceRequest;

template <typename K, typename V>
class Map;

template <typename T>
class ScopedHandleBase;

template <typename T>
class StructPtr;

namespace internal {

template <typename T, typename Enable = void>
struct ValueTraits {
  static bool Equals(const T& a, const T& b) { return a == b; }
};

template <typename T>
struct ValueTraits<
    T,
    typename EnableIf<IsSpecializationOf<Array, T>::value ||
                      IsSpecializationOf<Map, T>::value ||
                      IsSpecializationOf<StructPtr, T>::value ||
                      IsSpecializationOf<InlinedStructPtr, T>::value>::type> {
  static bool Equals(const T& a, const T& b) { return a.Equals(b); }
};

template <typename T>
struct ValueTraits<ScopedHandleBase<T>> {
  static bool Equals(const ScopedHandleBase<T>& a,
                     const ScopedHandleBase<T>& b) {
    return a.get().value() == b.get().value();
  }
};

template <typename T>
struct ValueTraits<
    T,
    typename EnableIf<
        IsSpecializationOf<InterfaceRequest, T>::value ||
        IsSpecializationOf<AssociatedInterfaceRequest, T>::value>::type> {
  static bool Equals(const T& a, const T& b) {
    if (&a == &b)
      return true;

    // Now that |a| and |b| refer to different objects, they are equivalent if
    // and only if they are both invalid.
    return !a.is_pending() && !b.is_pending();
  }
};

template <typename T>
struct ValueTraits<InterfacePtr<T>> {
  static bool Equals(const InterfacePtr<T>& a, const InterfacePtr<T>& b) {
    if (&a == &b)
      return true;

    // Now that |a| and |b| refer to different objects, they are equivalent if
    // and only if they are both null.
    return !a && !b;
  }
};

template <typename T>
struct ValueTraits<AssociatedInterfacePtrInfo<T>> {
  static bool Equals(const AssociatedInterfacePtrInfo<T>& a,
                     const AssociatedInterfacePtrInfo<T>& b) {
    if (&a == &b)
      return true;

    // Now that |a| and |b| refer to different objects, they are equivalent if
    // and only if they are both invalid.
    return !a.is_valid() && !b.is_valid();
  }
};

}  // namespace internal
}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_LIB_VALUE_TRAITS_H_

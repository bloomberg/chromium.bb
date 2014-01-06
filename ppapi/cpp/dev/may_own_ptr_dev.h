// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_CPP_DEV_MAY_OWN_PTR_DEV_H_
#define PPAPI_CPP_DEV_MAY_OWN_PTR_DEV_H_

#include "ppapi/cpp/logging.h"

namespace pp {

// An annotation used along with a pointer parameter of a function to indicate
// that the function doesn't take ownership of the pointer.
enum NotOwned {
  NOT_OWNED
};

namespace internal {

// MayOwnPtr keeps track of whether it has ownership of the pointer it holds,
// and deletes the pointer on destruction if it does.
template <typename T>
class MayOwnPtr {
 public:
  // Creates and owns a T instance.
  // NOTE: "()" after T is important to do zero-initialization for POD types.
  MayOwnPtr() : value_(new T()), owned_(true) {}

  // It doesn't take ownership of |value|, therefore |value| must live longer
  // than this object.
  MayOwnPtr(T* value, NotOwned) : value_(value), owned_(false) {
    PP_DCHECK(value);
  }

  ~MayOwnPtr() {
    if (owned_)
      delete value_;
  }

  const T* get() const { return value_; }
  T* get() { return value_; }

  const T& operator*() const { return *value_; }
  T& operator*() { return *value_; }

  const T* operator->() const { return value_; }
  T* operator->() { return value_; }

 private:
  // Disallow copying and assignment.
  MayOwnPtr(const MayOwnPtr<T>&);
  MayOwnPtr<T>& operator=(const MayOwnPtr<T>&);

  T* value_;
  bool owned_;
};

}  // namespace internal
}  // namespace pp

#endif  // PPAPI_CPP_DEV_MAY_OWN_PTR_DEV_H_

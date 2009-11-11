// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef O3D_GPU_PLUGIN_NP_UTILS_NP_OBJECT_POINTER_H_
#define O3D_GPU_PLUGIN_NP_UTILS_NP_OBJECT_POINTER_H_

#include "base/logging.h"
#include "o3d/gpu_plugin/np_utils/np_browser.h"
#include "o3d/gpu_plugin/np_utils/np_headers.h"

namespace gpu_plugin {

// Smart pointer for NPObjects that automatically handles reference counting.
template <typename NPObjectType>
class NPObjectPointer {
 public:
  NPObjectPointer() : object_(NULL) {}

  NPObjectPointer(const NPObjectPointer& rhs) : object_(rhs.object_) {
    Retain();
  }

  explicit NPObjectPointer(NPObjectType* p) : object_(p) {
    Retain();
  }

  template <typename RHS>
  NPObjectPointer(const NPObjectPointer<RHS>& rhs) : object_(rhs.Get()) {
    Retain();
  }

  ~NPObjectPointer() {
    Release();
  }

  NPObjectPointer& operator=(const NPObjectPointer& rhs) {
    if (object_ == rhs.Get())
      return *this;

    Release();
    object_ = rhs.object_;
    Retain();
    return *this;
  }

  template <typename RHS>
  NPObjectPointer& operator=(const NPObjectPointer<RHS>& rhs) {
    if (object_ == rhs.Get())
      return *this;

    Release();
    object_ = rhs.Get();
    Retain();
    return *this;
  }

  template <class RHS>
  bool operator==(const NPObjectPointer<RHS>& rhs) const {
    return object_ == rhs.Get();
  }

  template <class RHS>
  bool operator!=(const NPObjectPointer<RHS>& rhs) const {
    return object_ != rhs.Get();
  }

  // The NPObject convention for returning an NPObject pointer from a function
  // is that the caller is responsible for releasing the reference count.
  static NPObjectPointer FromReturned(NPObjectType* p) {
    NPObjectPointer pointer(p);
    pointer.Release();
    return pointer;
  }

  // The NPObject convention for returning an NPObject pointer from a function
  // is that the caller is responsible for releasing the reference count.
  NPObjectType* ToReturned() const {
    Retain();
    return object_;
  }

  NPObjectType* Get() const {
    return object_;
  }

  NPObjectType* operator->() const {
    return object_;
  }

  NPObjectType& operator*() const {
    return *object_;
  }

 private:
  void Retain() const {
    if (object_) {
      NPBrowser::get()->RetainObject(object_);
    }
  }

  void Release() const {
    if (object_) {
      NPBrowser::get()->ReleaseObject(object_);
    }
  }

  NPObjectType* object_;
};

// For test diagnostics.
template <typename NPObjectType>
std::ostream& operator<<(std::ostream& stream,
                         const NPObjectPointer<NPObjectType>& pointer) {
  return stream << pointer.Get();
}
}  // namespace gpu_plugin

#endif  // O3D_GPU_PLUGIN_NP_UTILS_NP_OBJECT_POINTER_H_

// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef O3D_GPU_PLUGIN_NP_UTILS_NP_OBJECT_POINTER_H_
#define O3D_GPU_PLUGIN_NP_UTILS_NP_OBJECT_POINTER_H_

#include "third_party/npapi/bindings/npapi.h"
#include "third_party/npapi/bindings/npruntime.h"

namespace o3d {
namespace gpu_plugin {

// Smart pointer for NPObjects that automatically handles reference counting.
template <typename NPObjectType>
class NPObjectPointer {
 public:
  NPObjectPointer() : object_(NULL) {}

  NPObjectPointer(const NPObjectPointer& rhs) : object_(rhs.object_) {
    if (object_) {
      NPN_RetainObject(object_);
    }
  }

  explicit NPObjectPointer(NPObjectType* p) : object_(p) {
    if (object_) {
      NPN_RetainObject(object_);
    }
  }

  ~NPObjectPointer() {
    if (object_) {
      NPN_ReleaseObject(object_);
    }
  }

  NPObjectPointer& operator=(const NPObjectPointer& rhs) {
    if (object_) {
      NPN_ReleaseObject(object_);
    }
    object_ = rhs.object_;
    if (object_) {
      NPN_RetainObject(object_);
    }
    return *this;
  }

  // The NPObject convention for returning an NPObject pointer from a function
  // is that the caller is responsible for releasing the reference count.
  static NPObjectPointer FromReturned(NPObjectType* p) {
    NPObjectPointer pointer(p);
    if (p) {
      NPN_ReleaseObject(p);
    }
    return pointer;
  }

  NPObjectType* Get() const {
    return object_;
  }

  NPObjectType* operator->() const {
    return object_;
  }

 private:
  NPObjectType* object_;
};

}  // namespace gpu_plugin
}  // namespace o3d

#endif  // O3D_GPU_PLUGIN_NP_UTILS_NP_OBJECT_POINTER_H_

// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_CPP_CORE_H_
#define PPAPI_CPP_CORE_H_

#include "ppapi/c/ppb_core.h"

namespace pp {

class CompletionCallback;
class Module;

// Simple wrapper around the PPB_Core interface. Some of these wrappers add
// nothing over the C interface, but some allow the use of C++ arguments.
class Core {
 public:
  // Note that we explicitly don't expose Resource& versions of this function
  // since Resource will normally manage the refcount properly. These should
  // be called only when doing manual management on raw PP_Resource handles,
  // which should be fairly rare.
  void AddRefResource(PP_Resource resource) {
    interface_->AddRefResource(resource);
  }
  void ReleaseResource(PP_Resource resource) {
    interface_->ReleaseResource(resource);
  }

  void* MemAlloc(size_t num_bytes) {
    return interface_->MemAlloc(num_bytes);
  }
  void MemFree(void* ptr) {
    interface_->MemFree(ptr);
  }

  PP_Time GetTime() {
    return interface_->GetTime();
  }

  PP_TimeTicks GetTimeTicks() {
    return interface_->GetTimeTicks();
  }

  void CallOnMainThread(int32_t delay_in_milliseconds,
                        const CompletionCallback& callback,
                        int32_t result = 0);

  bool IsMainThread();

 private:
  // Allow Module to construct.
  friend class Module;

  // Only module should make this class so this constructor is private.
  Core(const PPB_Core* inter) : interface_(inter) {}

  // Copy and assignment are disallowed.
  Core(const Core& other);
  Core& operator=(const Core& other);

  const PPB_Core* interface_;
};

}  // namespace pp

#endif  // PPAPI_CPP_CORE_H_

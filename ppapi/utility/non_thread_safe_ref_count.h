// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_UTILITY_NON_THREAD_SAFE_REF_COUNT_H_
#define PPAPI_UTILITY_NON_THREAD_SAFE_REF_COUNT_H_

#include "ppapi/cpp/core.h"
#include "ppapi/cpp/logging.h"
#include "ppapi/cpp/module.h"

/// @file
/// This file defines the APIs for maintaining a reference counter.
namespace pp {

/// A simple reference counter that is not thread-safe. <strong>Note:</strong>
/// in Debug mode, it checks that it is either called on the main thread, or
/// always called on another thread.
class NonThreadSafeRefCount {
 public:
  /// Default constructor. In debug mode, this checks that the object is being
  /// created on the main thread.
  NonThreadSafeRefCount()
      : ref_(0) {
#ifndef NDEBUG
    is_main_thread_ = Module::Get()->core()->IsMainThread();
#endif
  }

  /// Destructor.
  ~NonThreadSafeRefCount() {
    PP_DCHECK(is_main_thread_ == Module::Get()->core()->IsMainThread());
  }

  /// AddRef() increments the reference counter.
  ///
  /// @return An int32_t with the incremented reference counter.
  int32_t AddRef() {
    PP_DCHECK(is_main_thread_ == Module::Get()->core()->IsMainThread());
    return ++ref_;
  }

  /// Release() decrements the reference counter.
  ///
  /// @return An int32_t with the decremeneted reference counter.
  int32_t Release() {
    PP_DCHECK(is_main_thread_ == Module::Get()->core()->IsMainThread());
    return --ref_;
  }

 private:
  int32_t ref_;
#ifndef NDEBUG
  bool is_main_thread_;
#endif
};

}  // namespace pp

#endif  // PPAPI_UTILITY_NON_THREAD_SAFE_REF_COUNT_H_

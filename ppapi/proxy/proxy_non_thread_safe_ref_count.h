// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PROXY_PROXY_NON_THREAD_SAFE_REF_COUNT_H_
#define PPAPI_PROXY_PROXY_NON_THREAD_SAFE_REF_COUNT_H_

#include "base/message_loop.h"
#include "ppapi/cpp/completion_callback.h"

namespace ppapi {
namespace proxy {

// This class is just like ppapi/cpp/non_thread_safe_ref_count.h but rather
// than using pp::Module::core (which doesn't exist), it uses Chrome threads
// which do.
class ProxyNonThreadSafeRefCount {
 public:
  ProxyNonThreadSafeRefCount() : ref_(0) {
#ifndef NDEBUG
    message_loop_ = MessageLoop::current();
#endif
  }

  ~ProxyNonThreadSafeRefCount() {
    PP_DCHECK(message_loop_ == MessageLoop::current());
  }

  int32_t AddRef() {
    PP_DCHECK(message_loop_ == MessageLoop::current());
    return ++ref_;
  }

  int32_t Release() {
    PP_DCHECK(message_loop_ == MessageLoop::current());
    return --ref_;
  }

 private:
  int32_t ref_;
#ifndef NDEBUG
  MessageLoop* message_loop_;
#endif
};

}  // namespace proxy
}  // namespace ppapi

#endif  // PPAPI_PROXY_PROXY_NON_THREAD_SAFE_REF_COUNT_H_

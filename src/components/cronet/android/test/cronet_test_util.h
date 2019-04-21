// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CRONET_ANDROID_TEST_CRONET_TEST_UTIL_H_
#define COMPONENTS_CRONET_ANDROID_TEST_CRONET_TEST_UTIL_H_

#include <jni.h>
#include "base/android/jni_android.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/single_thread_task_runner.h"

namespace net {
class URLRequest;
class URLRequestContext;
}  // namespace net

namespace cronet {

// Various test utility functions for testing Cronet.
// NOTE(pauljensen): This class is friended by Cronet internal implementation
// classes to provide access to internals.
class TestUtil {
 public:
  // CronetURLRequestContextAdapter manipulation:

  // Returns SingleThreadTaskRunner for the network thread of the context
  // adapter.
  static scoped_refptr<base::SingleThreadTaskRunner> GetTaskRunner(
      jlong jcontext_adapter);
  // Returns underlying URLRequestContext.
  static net::URLRequestContext* GetURLRequestContext(jlong jcontext_adapter);
  // Run |task| after URLRequestContext is initialized.
  static void RunAfterContextInit(jlong jcontext_adapter,
                                  const base::Closure& task);

  // CronetURLRequestAdapter manipulation:

  // Returns underlying URLRequest.
  static net::URLRequest* GetURLRequest(jlong jrequest_adapter);

 private:
  static void RunAfterContextInitOnNetworkThread(jlong jcontext_adapter,
                                                 const base::Closure& task);

  DISALLOW_IMPLICIT_CONSTRUCTORS(TestUtil);
};

}  // namespace cronet

#endif  // COMPONENTS_CRONET_ANDROID_TEST_CRONET_TEST_UTIL_H_

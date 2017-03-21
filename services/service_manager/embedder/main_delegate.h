// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_SERVICE_MANAGER_EMBEDDER_MAIN_DELEGATE_H_
#define SERVICES_SERVICE_MANAGER_EMBEDDER_MAIN_DELEGATE_H_

#include "services/service_manager/embedder/service_manager_embedder_export.h"

namespace base {
namespace mac {
class ScopedNSAutoreleasePool;
}
}

namespace service_manager {

// An interface which must be implemented by Service Manager embedders to
// control basic process initialization and shutdown, as well as early branching
// to run specific types of subprocesses.
class MainDelegate {
 public:
  // Extra parameters passed to MainDelegate::Initialize.
  struct InitializeParams {
#if defined(OS_MACOSX)
    // The outermost autorelease pool, allocated by internal service manager
    // logic. This is guaranteed to live throughout the extent of Run().
    base::mac::ScopedNSAutoreleasePool* autorelease_pool = nullptr;
#endif
  };

  virtual ~MainDelegate() {}

  // Perform early process initialization. Returns -1 if successful, or the exit
  // code with which the process should be terminated due to initialization
  // failure.
  virtual int Initialize(const InitializeParams& params) = 0;

  // Runs the main process logic. Called exactly once, and only after a
  // successful call to Initialize(). Returns the exit code to use when
  // terminating the process after Run() (and then ShutDown()) completes.
  virtual int Run() = 0;

  // Called after Run() returns, before exiting the process.
  virtual void ShutDown() = 0;
};

}  // namespace service_manager

#endif  // SERVICES_SERVICE_MANAGER_EMBEDDER_MAIN_DELEGATE_H_

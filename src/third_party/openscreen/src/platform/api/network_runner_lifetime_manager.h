// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file

#ifndef PLATFORM_API_NETWORK_RUNNER_LIFETIME_MANAGER_H_
#define PLATFORM_API_NETWORK_RUNNER_LIFETIME_MANAGER_H_

#include "platform/api/network_runner.h"
#include "platform/api/task_runner.h"

namespace openscreen {
namespace platform {

class NetworkRunnerLifetimeManager {
 public:
  virtual ~NetworkRunnerLifetimeManager() = default;

  // Creates a new NetworkRunnerLifetimeManager
  // NOTE: The platform must implement this method if
  // NetworkRunnerLifetimeManager is to be used.
  static std::unique_ptr<NetworkRunnerLifetimeManager> Create();

  // Creates the NetworkRunner for this factory instance. This method must be
  // called no more than once.
  virtual void CreateNetworkRunner() = 0;

  // Returns a pointer to the NetworkRunner instance owned by this factory.
  virtual NetworkRunner* Get() = 0;
};

}  // namespace platform
}  // namespace openscreen

#endif  // PLATFORM_API_NETWORK_RUNNER_LIFETIME_MANAGER_H_

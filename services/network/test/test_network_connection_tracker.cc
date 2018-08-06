// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/test/test_network_connection_tracker.h"

#include "base/logging.h"
#include "base/threading/thread_task_runner_handle.h"

namespace network {

TestNetworkConnectionTracker::TestNetworkConnectionTracker(
    bool respond_synchronously,
    network::mojom::ConnectionType initial_type)
    : respond_synchronously_(respond_synchronously), type_(initial_type) {}

bool TestNetworkConnectionTracker::GetConnectionType(
    network::mojom::ConnectionType* type,
    ConnectionTypeCallback callback) {
  if (respond_synchronously_) {
    *type = type_;
    return true;
  }

  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), type_));
  return false;
}

void TestNetworkConnectionTracker::SetConnectionType(
    network::mojom::ConnectionType type) {
  type_ = type;
  OnNetworkChanged(type_);
}

}  // namespace network

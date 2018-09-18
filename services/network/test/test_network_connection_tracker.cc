// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/test/test_network_connection_tracker.h"

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/threading/thread_task_runner_handle.h"

namespace network {

static TestNetworkConnectionTracker* g_test_network_connection_tracker_instance;

// static
std::unique_ptr<TestNetworkConnectionTracker>
TestNetworkConnectionTracker::CreateInstance() {
  return base::WrapUnique(new TestNetworkConnectionTracker());
}

// static
TestNetworkConnectionTracker* TestNetworkConnectionTracker::GetInstance() {
  DCHECK(g_test_network_connection_tracker_instance);
  return g_test_network_connection_tracker_instance;
}

TestNetworkConnectionTracker::TestNetworkConnectionTracker() {
  if (g_test_network_connection_tracker_instance) {
    LOG(WARNING) << "Creating more than one TestNetworkConnectionTracker";
    return;
  }
  g_test_network_connection_tracker_instance = this;
}

TestNetworkConnectionTracker::~TestNetworkConnectionTracker() {
  if (g_test_network_connection_tracker_instance == this)
    g_test_network_connection_tracker_instance = nullptr;
}

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

void TestNetworkConnectionTracker::SetRespondSynchronously(
    bool respond_synchronously) {
  respond_synchronously_ = respond_synchronously;
}

}  // namespace network

// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/test/test_network_connection_tracker.h"

#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/threading/sequenced_task_runner_handle.h"

namespace network {

static TestNetworkConnectionTracker* g_test_network_connection_tracker_instance;

namespace {

using NetworkConnectionTrackerCallback =
    base::OnceCallback<void(NetworkConnectionTracker*)>;

void GetInstanceAsync(NetworkConnectionTrackerCallback callback) {
  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](NetworkConnectionTrackerCallback callback) {
            std::move(callback).Run(g_test_network_connection_tracker_instance);
          },
          std::move(callback)));
}

NetworkConnectionTracker* GetNonTestInstance() {
  return TestNetworkConnectionTracker::GetInstance();
}

}  // namespace

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

// static
NetworkConnectionTrackerGetter TestNetworkConnectionTracker::CreateGetter() {
  return base::BindRepeating(&GetNonTestInstance);
}

// static
NetworkConnectionTrackerAsyncGetter
TestNetworkConnectionTracker::CreateAsyncGetter() {
  return base::BindRepeating(&GetInstanceAsync);
}

TestNetworkConnectionTracker::TestNetworkConnectionTracker() {
  if (g_test_network_connection_tracker_instance) {
    LOG(WARNING) << "Creating more than one TestNetworkConnectionTracker";
    return;
  }
  g_test_network_connection_tracker_instance = this;

  // Make sure the real NetworkConnectionTracker thinks there's always a
  // connection available. GetConnectionType asynchronisity will be implemented
  // in the override in this class.
  OnNetworkChanged(network::mojom::ConnectionType::CONNECTION_UNKNOWN);
}

TestNetworkConnectionTracker::~TestNetworkConnectionTracker() {
  if (g_test_network_connection_tracker_instance == this)
    g_test_network_connection_tracker_instance = nullptr;
}

bool TestNetworkConnectionTracker::GetConnectionType(
    network::mojom::ConnectionType* type,
    ConnectionTypeCallback callback) {
  network::mojom::ConnectionType current_type;
  bool sync = NetworkConnectionTracker::GetConnectionType(&current_type,
                                                          base::DoNothing());
  DCHECK(sync);
  if (respond_synchronously_) {
    *type = current_type;
    return true;
  }

  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), current_type));
  return false;
}

void TestNetworkConnectionTracker::SetConnectionType(
    network::mojom::ConnectionType type) {
  OnNetworkChanged(type);
}

void TestNetworkConnectionTracker::SetRespondSynchronously(
    bool respond_synchronously) {
  respond_synchronously_ = respond_synchronously;
}

}  // namespace network

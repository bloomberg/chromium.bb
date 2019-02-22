// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/background_sync/background_sync_network_observer.h"

#include "base/run_loop.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "services/network/test/test_network_connection_tracker.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

class BackgroundSyncNetworkObserverTest : public testing::Test {
 public:
  BackgroundSyncNetworkObserverTest() : network_changed_count_(0) {
    network_observer_ =
        std::make_unique<BackgroundSyncNetworkObserver>(base::BindRepeating(
            &BackgroundSyncNetworkObserverTest::OnConnectionChanged,
            base::Unretained(this)));
  }

  void SetNetwork(network::mojom::ConnectionType connection_type) {
    network::TestNetworkConnectionTracker::GetInstance()->SetConnectionType(
        connection_type);
    base::RunLoop().RunUntilIdle();
  }

  void OnConnectionChanged() { network_changed_count_++; }

 protected:
  TestBrowserThreadBundle browser_thread_bundle_;

  std::unique_ptr<BackgroundSyncNetworkObserver> network_observer_;
  int network_changed_count_;
};

TEST_F(BackgroundSyncNetworkObserverTest, NetworkChangeInvokesCallback) {
  SetNetwork(network::mojom::ConnectionType::CONNECTION_NONE);
  network_changed_count_ = 0;

  SetNetwork(network::mojom::ConnectionType::CONNECTION_WIFI);
  EXPECT_EQ(1, network_changed_count_);
  SetNetwork(network::mojom::ConnectionType::CONNECTION_3G);
  EXPECT_EQ(2, network_changed_count_);
  SetNetwork(network::mojom::ConnectionType::CONNECTION_UNKNOWN);
  EXPECT_EQ(3, network_changed_count_);
  SetNetwork(network::mojom::ConnectionType::CONNECTION_NONE);
  EXPECT_EQ(4, network_changed_count_);
  SetNetwork(network::mojom::ConnectionType::CONNECTION_NONE);
  EXPECT_EQ(4, network_changed_count_);
}

TEST_F(BackgroundSyncNetworkObserverTest, NetworkSufficientAnyNetwork) {
  SetNetwork(network::mojom::ConnectionType::CONNECTION_WIFI);
  EXPECT_TRUE(network_observer_->NetworkSufficient(NETWORK_STATE_ANY));

  SetNetwork(network::mojom::ConnectionType::CONNECTION_3G);
  EXPECT_TRUE(network_observer_->NetworkSufficient(NETWORK_STATE_ANY));

  SetNetwork(network::mojom::ConnectionType::CONNECTION_UNKNOWN);
  EXPECT_TRUE(network_observer_->NetworkSufficient(NETWORK_STATE_ANY));

  SetNetwork(network::mojom::ConnectionType::CONNECTION_NONE);
  EXPECT_TRUE(network_observer_->NetworkSufficient(NETWORK_STATE_ANY));
}

TEST_F(BackgroundSyncNetworkObserverTest, NetworkSufficientAvoidCellular) {
  SetNetwork(network::mojom::ConnectionType::CONNECTION_WIFI);
  EXPECT_TRUE(
      network_observer_->NetworkSufficient(NETWORK_STATE_AVOID_CELLULAR));

  SetNetwork(network::mojom::ConnectionType::CONNECTION_UNKNOWN);
  EXPECT_TRUE(
      network_observer_->NetworkSufficient(NETWORK_STATE_AVOID_CELLULAR));

  SetNetwork(network::mojom::ConnectionType::CONNECTION_2G);
  EXPECT_FALSE(
      network_observer_->NetworkSufficient(NETWORK_STATE_AVOID_CELLULAR));

  SetNetwork(network::mojom::ConnectionType::CONNECTION_3G);
  EXPECT_FALSE(
      network_observer_->NetworkSufficient(NETWORK_STATE_AVOID_CELLULAR));

  SetNetwork(network::mojom::ConnectionType::CONNECTION_4G);
  EXPECT_FALSE(
      network_observer_->NetworkSufficient(NETWORK_STATE_AVOID_CELLULAR));

  SetNetwork(network::mojom::ConnectionType::CONNECTION_NONE);
  EXPECT_FALSE(
      network_observer_->NetworkSufficient(NETWORK_STATE_AVOID_CELLULAR));
}

TEST_F(BackgroundSyncNetworkObserverTest, ConditionsMetOnline) {
  SetNetwork(network::mojom::ConnectionType::CONNECTION_WIFI);
  EXPECT_TRUE(network_observer_->NetworkSufficient(NETWORK_STATE_ONLINE));

  SetNetwork(network::mojom::ConnectionType::CONNECTION_3G);
  EXPECT_TRUE(network_observer_->NetworkSufficient(NETWORK_STATE_ONLINE));

  SetNetwork(network::mojom::ConnectionType::CONNECTION_UNKNOWN);
  EXPECT_TRUE(network_observer_->NetworkSufficient(NETWORK_STATE_ONLINE));

  SetNetwork(network::mojom::ConnectionType::CONNECTION_NONE);
  EXPECT_FALSE(network_observer_->NetworkSufficient(NETWORK_STATE_ONLINE));
}

TEST_F(BackgroundSyncNetworkObserverTest, GetNetworkOnConstruction) {
  // We need to emulate being disconnected before creating the network observer.
  network_observer_.reset();
  SetNetwork(network::mojom::ConnectionType::CONNECTION_NONE);

  auto observer =
      std::make_unique<BackgroundSyncNetworkObserver>(base::BindRepeating(
          &BackgroundSyncNetworkObserverTest::OnConnectionChanged,
          base::Unretained(this)));

  base::RunLoop().RunUntilIdle();

  // The network observer should have learned that the current network is not
  // the default (UNKNOWN) but is in fact NONE.
  EXPECT_EQ(1, network_changed_count_);
  EXPECT_FALSE(observer->NetworkSufficient(NETWORK_STATE_ONLINE));
}

}  // namespace content

// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/notifier/gcm_network_channel.h"

#include "base/compiler_specific.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {
namespace {

class GCMNetworkChannelTest
    : public ::testing::Test,
      public SyncNetworkChannel::Observer {
 protected:
  GCMNetworkChannelTest()
      : gcm_network_channel_() {
    gcm_network_channel_.AddObserver(this);
    gcm_network_channel_.SetMessageReceiver(
        invalidation::NewPermanentCallback(
            this, &GCMNetworkChannelTest::OnIncomingMessage));
  }

  virtual ~GCMNetworkChannelTest() {
    gcm_network_channel_.RemoveObserver(this);
  }

  virtual void OnNetworkChannelStateChanged(
      InvalidatorState invalidator_state) OVERRIDE {
  }

  void OnIncomingMessage(std::string incoming_message) {
  }

  GCMNetworkChannel gcm_network_channel_;
};

}  // namespace
}  // namespace syncer

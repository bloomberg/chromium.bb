// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/test/fake_network_manager.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "jingle/glue/utils.h"
#include "third_party/webrtc/base/socketaddress.h"

namespace remoting {

FakeNetworkManager::FakeNetworkManager(const rtc::IPAddress& address)
    : started_(false),
      weak_factory_(this) {
  network_.reset(new rtc::Network("fake", "Fake Network", address, 32));
  network_->AddIP(address);
}

FakeNetworkManager::~FakeNetworkManager() {
}

void FakeNetworkManager::StartUpdating() {
  started_ = true;
  base::MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(&FakeNetworkManager::SendNetworksChangedSignal,
                 weak_factory_.GetWeakPtr()));
}

void FakeNetworkManager::StopUpdating() {
  started_ = false;
}

void FakeNetworkManager::GetNetworks(NetworkList* networks) const {
  networks->clear();
  networks->push_back(network_.get());
}

void FakeNetworkManager::SendNetworksChangedSignal() {
  SignalNetworksChanged();
}

}  // namespace remoting

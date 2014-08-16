// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_TEST_FAKE_NETWORK_MANAGER_H_
#define REMOTING_TEST_FAKE_NETWORK_MANAGER_H_

#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "third_party/webrtc/base/network.h"

namespace remoting {

// FakeNetworkManager always returns one interface with the IP address
// specified in the constructor.
class FakeNetworkManager : public rtc::NetworkManager {
 public:
  FakeNetworkManager(const rtc::IPAddress& address);
  virtual ~FakeNetworkManager();

  // rtc::NetworkManager interface.
  virtual void StartUpdating() OVERRIDE;
  virtual void StopUpdating() OVERRIDE;
  virtual void GetNetworks(NetworkList* networks) const OVERRIDE;

 protected:
  void SendNetworksChangedSignal();

  bool started_;
  scoped_ptr<rtc::Network> network_;

  base::WeakPtrFactory<FakeNetworkManager> weak_factory_;
};

}  // namespace remoting

#endif  // REMOTING_TEST_FAKE_NETWORK_MANAGER_H_

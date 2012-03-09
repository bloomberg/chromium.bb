// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_NETWORK_LIST_OBSERVER_H_
#define WEBKIT_GLUE_NETWORK_LIST_OBSERVER_H_

#include <vector>

namespace net {
struct NetworkInterface;
typedef std::vector<NetworkInterface> NetworkInterfaceList;
}  // namespace net

namespace webkit_glue {

class NetworkListObserver {
 public:
  virtual ~NetworkListObserver() {}

  virtual void OnNetworkListChanged(
      const net::NetworkInterfaceList& list) = 0;

 protected:
  NetworkListObserver() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(NetworkListObserver);
};

}  // namespace webkit_glue

#endif  // WEBKIT_GLUE_NETWORK_LIST_OBSERVER_H_

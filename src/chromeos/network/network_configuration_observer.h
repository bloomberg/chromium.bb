// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_NETWORK_NETWORK_CONFIGURATION_OBSERVER_H_
#define CHROMEOS_NETWORK_NETWORK_CONFIGURATION_OBSERVER_H_

#include <string>

namespace chromeos {

// Observer class for network configuration events (remove only).
class NetworkConfigurationObserver {
 public:
  // Called whenever a network configuration is removed. |service_path|
  // provides the Shill current identifier for the network. |guid| will be set
  // to the corresponding GUID for the network if known at the time of removal,
  // otherwise it will be empty.
  virtual void OnConfigurationRemoved(const std::string& service_path,
                                      const std::string& guid) = 0;

 protected:
  virtual ~NetworkConfigurationObserver() {}

 private:
  DISALLOW_ASSIGN(NetworkConfigurationObserver);
};

}  // namespace chromeos

#endif  // CHROMEOS_NETWORK_NETWORK_CONFIGURATION_OBSERVER_H_

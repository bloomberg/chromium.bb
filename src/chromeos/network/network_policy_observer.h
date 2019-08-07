// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_NETWORK_NETWORK_POLICY_OBSERVER_H_
#define CHROMEOS_NETWORK_NETWORK_POLICY_OBSERVER_H_

#include <string>


namespace chromeos {

class NetworkPolicyObserver {
 public:
  // Called when the policies for |userhash| were set (also when they were
  // updated). An empty |userhash| designates the device policy.
  // Note that the policies might be empty and might not have been applied yet
  // at that time.
  virtual void PoliciesChanged(const std::string& userhash) {}

  // Called every time a policy application for |userhash| finished. This is
  // only called once no more policies are pending for |userhash|.
  virtual void PoliciesApplied(const std::string& userhash) {}

  // Called every time a network is created or updated because of a policy.
  virtual void PolicyAppliedToNetwork(const std::string& service_path) {}

 protected:
  virtual ~NetworkPolicyObserver() {}

 private:
  DISALLOW_ASSIGN(NetworkPolicyObserver);
};

}  // namespace chromeos

#endif  // CHROMEOS_NETWORK_NETWORK_POLICY_OBSERVER_H_

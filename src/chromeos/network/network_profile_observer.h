// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_NETWORK_NETWORK_PROFILE_OBSERVER_H_
#define CHROMEOS_NETWORK_NETWORK_PROFILE_OBSERVER_H_

#include <string>


namespace chromeos {

struct NetworkProfile;

class NetworkProfileObserver {
 public:
  virtual void OnProfileAdded(const NetworkProfile& profile) = 0;
  virtual void OnProfileRemoved(const NetworkProfile& profile) = 0;

 protected:
  virtual ~NetworkProfileObserver() {}

 private:
  DISALLOW_ASSIGN(NetworkProfileObserver);
};

}  // namespace chromeos

#endif  // CHROMEOS_NETWORK_NETWORK_PROFILE_OBSERVER_H_

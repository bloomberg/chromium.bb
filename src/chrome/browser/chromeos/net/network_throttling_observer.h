// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_NET_NETWORK_THROTTLING_OBSERVER_H_
#define CHROME_BROWSER_CHROMEOS_NET_NETWORK_THROTTLING_OBSERVER_H_

#include <string>

#include "base/memory/weak_ptr.h"
#include "components/prefs/pref_change_registrar.h"

class PrefRegistrySimple;

namespace chromeos {

// NetworkThrottlingObserver is a singleton, owned by
// ChromeBrowserMainPartsChromeos.
// This class is responsible for propagating network bandwidth throttling policy
// changes (prefs::kNetworkThrottlingEnabled) in Chrome down to Shill which
// implements by calling 'tc' in the kernel.
class NetworkThrottlingObserver {
 public:
  explicit NetworkThrottlingObserver(PrefService* local_state);
  ~NetworkThrottlingObserver();

  static void RegisterPrefs(PrefRegistrySimple* registry);

 private:
  // Callback used when prefs::kNetworkThrottlingEnabled changes
  void OnPreferenceChanged(const std::string& pref_name);

  PrefService* local_state_;
  PrefChangeRegistrar pref_change_registrar_;

  DISALLOW_COPY_AND_ASSIGN(NetworkThrottlingObserver);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_NET_NETWORK_THROTTLING_OBSERVER_H_

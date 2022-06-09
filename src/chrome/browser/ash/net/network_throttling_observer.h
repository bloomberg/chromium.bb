// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_NET_NETWORK_THROTTLING_OBSERVER_H_
#define CHROME_BROWSER_ASH_NET_NETWORK_THROTTLING_OBSERVER_H_

#include <string>

#include "base/memory/weak_ptr.h"
#include "components/prefs/pref_change_registrar.h"

class PrefRegistrySimple;

namespace chromeos {

// NetworkThrottlingObserver is a singleton, owned by
// `ChromeBrowserMainPartsAsh`.
// This class is responsible for propagating network bandwidth throttling policy
// changes (prefs::kNetworkThrottlingEnabled) in Chrome down to Shill which
// implements by calling 'tc' in the kernel.
class NetworkThrottlingObserver {
 public:
  explicit NetworkThrottlingObserver(PrefService* local_state);

  NetworkThrottlingObserver(const NetworkThrottlingObserver&) = delete;
  NetworkThrottlingObserver& operator=(const NetworkThrottlingObserver&) =
      delete;

  ~NetworkThrottlingObserver();

  static void RegisterPrefs(PrefRegistrySimple* registry);

 private:
  // Callback used when prefs::kNetworkThrottlingEnabled changes
  void OnPreferenceChanged(const std::string& pref_name);

  PrefService* local_state_;
  PrefChangeRegistrar pref_change_registrar_;
};

}  // namespace chromeos

// TODO(https://crbug.com/1164001): remove after the migration is finished.
namespace ash {
using ::chromeos::NetworkThrottlingObserver;
}

#endif  // CHROME_BROWSER_ASH_NET_NETWORK_THROTTLING_OBSERVER_H_

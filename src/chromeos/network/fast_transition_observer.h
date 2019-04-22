// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_NETWORK_FAST_TRANSITION_OBSERVER_H_
#define CHROMEOS_NETWORK_FAST_TRANSITION_OBSERVER_H_

#include <string>

#include "base/component_export.h"
#include "base/memory/weak_ptr.h"
#include "components/prefs/pref_change_registrar.h"

class PrefRegistrySimple;

namespace chromeos {

// FastTransitionObserver is a singleton, owned by
// ChromeBrowserMainPartsChromeos.
// This class is responsible for propagating Fast Transition policy
// changes (prefs::kFastTransitionEnabled) in Chrome down to Shill.
class COMPONENT_EXPORT(CHROMEOS_NETWORK) FastTransitionObserver {
 public:
  explicit FastTransitionObserver(PrefService* local_state);
  ~FastTransitionObserver();

  static void RegisterPrefs(PrefRegistrySimple* registry);

 private:
  // Callback used when prefs::kFastTransitionEnabled changes
  void OnPreferenceChanged(const std::string& pref_name);

  PrefService* local_state_;
  PrefChangeRegistrar pref_change_registrar_;

  DISALLOW_COPY_AND_ASSIGN(FastTransitionObserver);
};

}  // namespace chromeos

#endif  // CHROMEOS_NETWORK_FAST_TRANSITION_OBSERVER_H_

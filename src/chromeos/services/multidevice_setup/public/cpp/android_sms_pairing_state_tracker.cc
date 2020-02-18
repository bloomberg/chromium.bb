// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/multidevice_setup/public/cpp/android_sms_pairing_state_tracker.h"

namespace chromeos {

namespace multidevice_setup {

AndroidSmsPairingStateTracker::AndroidSmsPairingStateTracker() = default;

AndroidSmsPairingStateTracker::~AndroidSmsPairingStateTracker() = default;

void AndroidSmsPairingStateTracker::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void AndroidSmsPairingStateTracker::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

void AndroidSmsPairingStateTracker::NotifyPairingStateChanged() {
  for (auto& observer : observer_list_)
    observer.OnPairingStateChanged();
}

}  // namespace multidevice_setup

}  // namespace chromeos

// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_MULTIDEVICE_SETUP_PUBLIC_CPP_FAKE_ANDROID_SMS_PAIRING_STATE_TRACKER_H_
#define CHROMEOS_SERVICES_MULTIDEVICE_SETUP_PUBLIC_CPP_FAKE_ANDROID_SMS_PAIRING_STATE_TRACKER_H_

#include "base/macros.h"
#include "chromeos/services/multidevice_setup/public/cpp/android_sms_pairing_state_tracker.h"

namespace chromeos {
namespace multidevice_setup {

class FakeAndroidSmsPairingStateTracker : public AndroidSmsPairingStateTracker {
 public:
  FakeAndroidSmsPairingStateTracker();
  ~FakeAndroidSmsPairingStateTracker() override;
  void SetPairingComplete(bool is_pairing_complete);

  // AndroidSmsPairingStateTracker:
  bool IsAndroidSmsPairingComplete() override;

 private:
  bool is_pairing_complete_ = false;

  DISALLOW_COPY_AND_ASSIGN(FakeAndroidSmsPairingStateTracker);
};

}  // namespace multidevice_setup
}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_MULTIDEVICE_SETUP_PUBLIC_CPP_FAKE_ANDROID_SMS_PAIRING_STATE_TRACKER_H_

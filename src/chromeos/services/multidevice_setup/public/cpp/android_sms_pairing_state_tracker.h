// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_MULTIDEVICE_SETUP_PUBLIC_CPP_ANDROID_SMS_PAIRING_STATE_TRACKER_H_
#define CHROMEOS_SERVICES_MULTIDEVICE_SETUP_PUBLIC_CPP_ANDROID_SMS_PAIRING_STATE_TRACKER_H_

#include "base/macros.h"
#include "base/observer_list.h"

namespace chromeos {
namespace multidevice_setup {

// Inspects and track pairing state of the Messages for Web PWA.
class AndroidSmsPairingStateTracker {
 public:
  class Observer : public base::CheckedObserver {
   public:
    virtual void OnPairingStateChanged() = 0;

   protected:
    ~Observer() override = default;
  };

  AndroidSmsPairingStateTracker();
  virtual ~AndroidSmsPairingStateTracker();

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Checks if Android Messages pairing has been completed.
  virtual bool IsAndroidSmsPairingComplete() = 0;

 protected:
  void NotifyPairingStateChanged();

 private:
  base::ObserverList<Observer> observer_list_;

  DISALLOW_COPY_AND_ASSIGN(AndroidSmsPairingStateTracker);
};

}  // namespace multidevice_setup
}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_MULTIDEVICE_SETUP_PUBLIC_CPP_ANDROID_SMS_PAIRING_STATE_TRACKER_H_

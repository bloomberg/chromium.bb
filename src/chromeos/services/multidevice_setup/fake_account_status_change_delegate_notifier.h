// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_MULTIDEVICE_SETUP_FAKE_ACCOUNT_STATUS_CHANGE_DELEGATE_NOTIFIER_H_
#define CHROMEOS_SERVICES_MULTIDEVICE_SETUP_FAKE_ACCOUNT_STATUS_CHANGE_DELEGATE_NOTIFIER_H_

#include "base/macros.h"
#include "chromeos/services/multidevice_setup/account_status_change_delegate_notifier.h"
#include "chromeos/services/multidevice_setup/public/mojom/multidevice_setup.mojom.h"

namespace chromeos {

namespace multidevice_setup {

// Test AccountStatusChangeDelegateNotifier implementation.
class FakeAccountStatusChangeDelegateNotifier
    : public AccountStatusChangeDelegateNotifier {
 public:
  FakeAccountStatusChangeDelegateNotifier() = default;
  ~FakeAccountStatusChangeDelegateNotifier() override = default;

  using AccountStatusChangeDelegateNotifier::delegate;

 private:
  DISALLOW_COPY_AND_ASSIGN(FakeAccountStatusChangeDelegateNotifier);
};

}  // namespace multidevice_setup

}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_MULTIDEVICE_SETUP_FAKE_ACCOUNT_STATUS_CHANGE_DELEGATE_NOTIFIER_H_

// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/app_mode/kiosk_app_launcher.h"

namespace chromeos {

KioskAppLauncher::KioskAppLauncher() = default;

KioskAppLauncher::KioskAppLauncher(KioskAppLauncher::Delegate* delegate)
    : delegate_(delegate) {}

void KioskAppLauncher::SetDelegate(Delegate* delegate) {
  delegate_ = delegate;
}

}  // namespace chromeos

// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/android_sms/android_sms_app_manager.h"

#include "url/gurl.h"

namespace chromeos {

namespace android_sms {

AndroidSmsAppManager::AndroidSmsAppManager() = default;

AndroidSmsAppManager::~AndroidSmsAppManager() = default;

void AndroidSmsAppManager::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void AndroidSmsAppManager::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

void AndroidSmsAppManager::NotifyInstalledAppUrlChanged() {
  for (auto& observer : observer_list_)
    observer.OnInstalledAppUrlChanged();
}

}  // namespace android_sms

}  // namespace chromeos

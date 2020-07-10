// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_TPM_FIRMWARE_UPDATE_NOTIFICATION_H_
#define CHROME_BROWSER_CHROMEOS_TPM_FIRMWARE_UPDATE_NOTIFICATION_H_

class Profile;

namespace chromeos {
namespace tpm_firmware_update {

// Displays a message that informs the user about a pending TPM Firmware Update,
// direction the user to the about page to trigger the update and allowing the
// notification to be silenced.
void ShowNotificationIfNeeded(Profile* profile);

}  // namespace tpm_firmware_update
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_TPM_FIRMWARE_UPDATE_NOTIFICATION_H_

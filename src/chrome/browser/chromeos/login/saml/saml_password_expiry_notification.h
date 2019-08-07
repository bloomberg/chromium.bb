// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_SAML_SAML_PASSWORD_EXPIRY_NOTIFICATION_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_SAML_SAML_PASSWORD_EXPIRY_NOTIFICATION_H_

class Profile;

namespace chromeos {

// Utility functions to show or hide a password expiry notification.

// Show a password expiry notification if the user's password has expired or
// soon expires (that is, within pref kSamlPasswordExpirationAdvanceWarningDays
// time). Otherwise, if the user's password will expire in the more distant
// future, in that case a notification will be shown in the future. Nothing is
// shown if the password is not expected to expire.
void MaybeShowSamlPasswordExpiryNotification(Profile* profile);

// Shows a password expiry notification. |less_than_n_days| should be 1 if the
// password expires in less than 1 day, 0 if it has already expired, etc.
// Negative numbers are treated the same as zero.
void ShowSamlPasswordExpiryNotification(Profile* profile, int less_than_n_days);

// Hides the password expiry notification if it is currently shown.
void DismissSamlPasswordExpiryNotification(Profile* profile);

// Stop waiting for the password to expire and free up any resources.
void ResetSamlPasswordExpiryNotificationForTesting();

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_SAML_SAML_PASSWORD_EXPIRY_NOTIFICATION_H_

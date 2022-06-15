// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_COMMON_PASSWORD_MANAGER_PREF_NAMES_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_COMMON_PASSWORD_MANAGER_PREF_NAMES_H_

#include "build/build_config.h"

namespace password_manager {
namespace prefs {

// Alphabetical list of preference names specific to the PasswordManager
// component.

// Boolean controlling whether the password manager allows automatic signing in
// through Credential Management API.
//
// IMPORTANT: This pref is neither querried nor updated on Android if the
// unified password manager is enabled.
// Use `password_manager_util::IsAutoSignInEnabled` to check
// the value of this setting instead.
extern const char kCredentialsEnableAutosignin[];

// The value of this preference controls whether the Password Manager will save
// credentials. When it is false, it doesn't ask if you want to save passwords
// but will continue to fill passwords.
//
// IMPORTANT: This pref is neither querried nor updated on Android if the
// unified password manager is enabled.
// Use `password_manager_util::IsSavingPasswordsEnabled` to check the value of
// this setting instead.
extern const char kCredentialsEnableService[];

#if BUILDFLAG(IS_ANDROID)
// Boolean controlling whether the password manager allows automatic signing in
// through Credential Management API. This pref is not synced. Its value is set
// by fetching the latest value from Google Mobile Services. Except for
// migration steps, it should not be modified in Chrome.
extern const char kAutoSignInEnabledGMS[];

// Boolean controlling whether the password manager offers to save passwords.
// If false, the password manager will not save credentials, but it will still
// fill previously saved ones. This pref is not synced. Its value is set
// by fetching the latest value from Google Mobile Services. Except for
// migration steps, it should not be modified in Chrome.
//
// This pref doesn't have a policy mapped to it directly, instead, the policy
// mapped to `kCredentialEnableService` will be applied.
extern const char kOfferToSavePasswordsEnabledGMS[];

// Boolean value indicating whether the regular prefs were migrated to UPM
// settings.
extern const char kSettingsMigratedToUPM[];

// Integer value which indicates the version used to migrate passwords from
// built in storage to Google Mobile Services.
extern const char kCurrentMigrationVersionToGoogleMobileServices[];

// Timestamps of when credentials from the GMS Core to the built in storage were
// last time migrated, in microseconds since Windows epoch.
extern const char kTimeOfLastMigrationAttempt[];

// Boolean value that indicated the need of data migration between the two
// backends due to sync settings change.
extern const char kRequiresMigrationAfterSyncStatusChange[];

// Boolean value indicating if the user has clicked on the "Password Manager"
// item in settings after switching to the Unified Password Manager. A "New"
// label is shown for the users who have not clicked on this item yet.
// TODO(crbug.com/1217070): Remove this once the feature is rolled out.
extern const char kPasswordsPrefWithNewLabelUsed[];

// Boolean value indicating if the user should not get UPM experience because
// of user-unresolvable errors received on communication with Google Mobile
// Services.
extern const char kUnenrolledFromGoogleMobileServicesDueToErrors[];
#endif

#if BUILDFLAG(IS_WIN)
// Whether the password was blank, only valid if OS password was last changed
// on or before the value contained in kOsPasswordLastChanged.
extern const char kOsPasswordBlank[];

// The number of seconds since epoch that the OS password was last changed.
extern const char kOsPasswordLastChanged[];
#endif

#if BUILDFLAG(IS_APPLE)
// The current status of migrating the passwords from the Keychain to the
// database. Stores a value from MigrationStatus.
extern const char kKeychainMigrationStatus[];
#endif

// Boolean that indicated whether first run experience for the auto sign-in
// prompt was shown or not.
extern const char kWasAutoSignInFirstRunExperienceShown[];

// Boolean that indicated whether one time removal of old google.com logins was
// performed.
extern const char kWereOldGoogleLoginsRemoved[];

// A dictionary of account-storage-related settings that exist per Gaia account
// (e.g. whether that user has opted in). It maps from hash of Gaia ID to
// dictionary of key-value pairs.
extern const char kAccountStoragePerAccountSettings[];

// String that represents the sync password hash.
extern const char kSyncPasswordHash[];

// String that represents the sync password length and salt. Its format is
// encrypted and converted to base64 string "<password length, as ascii
// int>.<16 char salt>".
extern const char kSyncPasswordLengthAndHashSalt[];

// Indicates the time (in seconds) when last cleaning of obsolete HTTP
// credentials was performed.
extern const char kLastTimeObsoleteHttpCredentialsRemoved[];

// The last time the password check has run to completion.
extern const char kLastTimePasswordCheckCompleted[];

// Timestamps of when password store metrics where last reported, in
// microseconds since Windows epoch.
extern const char kLastTimePasswordStoreMetricsReported[];

// The last time the password check has run to completion synced across devices.
// It's used on passwords.google.com and not in Chrome.
extern const char kSyncedLastTimePasswordCheckCompleted[];

// List that contains captured password hashes.
extern const char kPasswordHashDataList[];

// Boolean indicating whether Chrome should check whether the credentials
// submitted by the user were part of a leak.
extern const char kPasswordLeakDetectionEnabled[];

// Boolean indicating whether users can mute (aka dismiss) alerts resulting from
// compromised credentials that were submitted by the user.
extern const char kPasswordDismissCompromisedAlertEnabled[];

// Timestamps of when credentials from the profile / account store were last
// used to fill a form, in microseconds since Windows epoch.
extern const char kProfileStoreDateLastUsedForFilling[];
extern const char kAccountStoreDateLastUsedForFilling[];

// A list of ongoing PasswordChangeSuccessTracker flows that is persisted in
// case Chrome is temporarily shut down while, e.g., a user retrieves a
// password reset email.
extern const char kPasswordChangeSuccessTrackerFlows[];

// Integer indicating the format version of the list saved under
// |kPasswordChangeSuccessTrackerFlows|.
extern const char kPasswordChangeSuccessTrackerVersion[];

}  // namespace prefs
}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_COMMON_PASSWORD_MANAGER_PREF_NAMES_H_

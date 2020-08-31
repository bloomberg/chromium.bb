// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CREDENTIAL_PROVIDER_COMMON_GCP_STRINGS_H_
#define CHROME_CREDENTIAL_PROVIDER_COMMON_GCP_STRINGS_H_

#include <string>
#include <vector>

namespace credential_provider {

// The the UI process can exit with the following exit code.
enum UiExitCodes {
  // The user completed the sign in successfully.
  kUiecSuccess,

  // The sign in was aborted by the user.
  kUiecAbort,

  // The sign in timed out.
  kUiecTimeout,

  // The process was killed by the GCP.
  kUiecKilled,

  // The email is not valid for the given user logging in.
  kUiecEMailMissmatch,

  // The email domain is not an accepted domain.
  kUiecInvalidEmailDomain,

  // Some signin data was missing.
  kUiecMissingSigninData,

  kUiecCount,
};

// Names of keys returned on json data from UI process.
extern const char kKeyEmail[];
extern const char kKeyPicture[];
extern const char kKeyFullname[];
extern const char kKeyId[];
extern const char kKeyMdmUrl[];
extern const char kKeyMdmIdToken[];
extern const char kKeyPassword[];
extern const char kKeyRefreshToken[];
extern const char kKeyAccessToken[];
extern const char kKeyMdmAccessToken[];
extern const char kKeySID[];
extern const char kKeyTokenHandle[];
extern const char kKeyUsername[];
extern const char kKeyDomain[];
extern const char kKeyExitCode[];

// AD attributes related to the device.
extern const char kKeyIsAdJoinedUser[];

// Name of registry value that holds user properties.
extern const wchar_t kUserTokenHandle[];
extern const wchar_t kUserEmail[];
extern const wchar_t kUserId[];
extern const wchar_t kUserPictureUrl[];

// Username and password key for special GAIA account to run GLS.
extern const wchar_t kDefaultGaiaAccountName[];
extern const wchar_t kLsaKeyGaiaUsername[];
extern const wchar_t kLsaKeyGaiaPassword[];

// Name of the desktop used on the Window welcome screen for interactive
// logon.
extern const wchar_t kDesktopName[];
extern const wchar_t kDesktopFullName[];

// Google Update related registry paths.
extern const wchar_t kRegUpdaterClientStateAppPath[];
extern const wchar_t kRegUpdaterClientsAppPath[];
extern const wchar_t kRegUninstallStringField[];
extern const wchar_t kRegUninstallArgumentsField[];
extern const wchar_t kRegUsageStatsName[];

// These are command line switches passed to chrome to start it as a process
// used as a logon stub.
extern const char kGcpwSigninSwitch[];
extern const char kPrefillEmailSwitch[];
extern const char kEmailDomainsSwitch[];
extern const char kGaiaIdSwitch[];
extern const char kGcpwEndpointPathSwitch[];
extern const char kGcpwAdditionalOauthScopes[];
extern const char kShowTosSwitch[];

// Parameter appended to sign in URL to pass valid signin domains to the inline
// login handler. These domains are separated by ','.
extern const char kEmailDomainsSigninPromoParameter[];
extern const char kEmailDomainsSeparator[];
extern const char kValidateGaiaIdSigninPromoParameter[];
extern const char kGcpwEndpointPathPromoParameter[];

// Crashpad related constants.
extern const wchar_t kRunAsCrashpadHandlerEntryPoint[];

// Flags to manipulate behavior of Chrome when importing credentials for the
// account signs in through GCPW.
extern const wchar_t kAllowImportOnlyOnFirstRun[];
extern const wchar_t kAllowImportWhenPrimaryAccountExists[];

// HKCU account information path in the hive of the OS user.
extern const wchar_t kRegHkcuAccountsPath[];

}  // namespace credential_provider

#endif  // CHROME_CREDENTIAL_PROVIDER_COMMON_GCP_STRINGS_H_

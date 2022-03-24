// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/login_pref_names.h"

namespace ash {
namespace prefs {

// *************** PROFILE PREFS ***************
// These are attached to the user profile. Here only login/oobe specific prefs
// are presented.

// Last input user method which could be used on the login/lock screens.
const char kLastLoginInputMethod[] = "login.last_input_method";

// A boolean pref to indicate if the marketing opt-in screen in OOBE is finished
// for the user.
const char kOobeMarketingOptInScreenFinished[] =
    "OobeMarketingOptInScreenFinished";

// Whether the user has chosen to sign up for marketing emails.
const char kOobeMarketingOptInChoice[] = "OobeMarketingOptInChoice";

// Time when new user has finished onboarding.
const char kOobeOnboardingTime[] = "oobe.onboarding_time";

// Indicates the amount of time for which a user authenticated against GAIA
// without SAML can use offline authentication against a cached password
// before being forced to go through online authentication against GAIA again.
// The time is expressed in days. A value of -1 indicates no limit, meaning
// that this policy will not enforce online authentication. The limit is in
// effect only if GAIA without SAML is used.
const char kGaiaOfflineSigninTimeLimitDays[] = "gaia.offline_signin_time_limit";

// A preference to keep track of the last time the user authenticated against
// GAIA without using SAML. The preference is updated whenever the user
// authenticates against GAIA: If GAIA performs the authentication itself, the
// preference is set to the current time. If GAIA redirects to a SAML IdP, the
// preference is cleared. The time is expressed as the serialization obtained
// from PrefService::SetTime().
const char kGaiaLastOnlineSignInTime[] = "gaia.last_online_sign_in_time";

// Indicates the amount of time for which a user authenticated via SAML can use
// offline authentication against a cached password before being forced to go
// through online authentication against GAIA again. The time is expressed in
// seconds. A value of -1 indicates no limit, meaning that this policy will not
// enforce online authentication. The limit is in effect only if GAIA redirected
// the user to a SAML IdP during the last online authentication.
const char kSAMLOfflineSigninTimeLimit[] = "saml.offline_signin_time_limit";

// A preference to keep track of the last time the user authenticated against
// GAIA using SAML. The preference is updated whenever the user authenticates
// against GAIA: If GAIA redirects to a SAML IdP, the preference is set to the
// current time. If GAIA performs the authentication itself, the preference is
// cleared. The time is expressed as the serialization obtained from
// PrefService::SetTime().
const char kSAMLLastGAIASignInTime[] = "saml.last_gaia_sign_in_time";

// Indicates the amount of time for which a user authenticated via GAIA
// without SAML can use offline authentication against a cached password
// before being forced to go through online authentication against GAIA again
// when logging in through the lock screen. The time is expressed in days. A
// value of -1 indicates no limit, meaning that this policy will not enforce
// online authentication. The limit is in effect only if GAIA without SAML is
// used.
const char kGaiaLockScreenOfflineSigninTimeLimitDays[] =
    "gaia.lock_screen_offline_signin_time_limit";

// Indicates the amount of time for which a user authenticated via SAML can use
// offline authentication against a cached password before being forced to go
// through online authentication against GAIA again when logging in through the
// lock screen. The time is expressed in days. A value of -1 indicates no limit,
// meaning that this policy will not enforce online authentication. The limit is
// in effect only if GAIA redirected the user to a SAML IdP during the last
// online authentication.
const char kSamlLockScreenOfflineSigninTimeLimitDays[] =
    "saml.lock_screen_offline_signin_time_limit";

// Enable chrome://password-change page for in-session change of SAML passwords.
// Also enables SAML password expiry notifications, if we have that information.
const char kSamlInSessionPasswordChangeEnabled[] =
    "saml.in_session_password_change_enabled";

// The number of days in advance to notify the user that their SAML password
// will expire (works when kSamlInSessionPasswordChangeEnabled is true).
const char kSamlPasswordExpirationAdvanceWarningDays[] =
    "saml.password_expiration_advance_warning_days";

// Enable online signin on the lock screen.
const char kLockScreenReauthenticationEnabled[] =
    "lock_screen_reauthentication_enabled";

// SAML password sync token fetched from the external API.
const char kSamlPasswordSyncToken[] = "saml.password_sync_token";

const char kActivityTimeAfterOnboarding[] =
    "oobe.activity_time_after_onboarding";

// *************** OOBE LOCAL STATE PREFS ***************

// A boolean pref of the OOBE complete flag (first OOBE part before login).
const char kOobeComplete[] = "OobeComplete";

// The name of the screen that has to be shown if OOBE has been interrupted.
const char kOobeScreenPending[] = "OobeScreenPending";

}  // namespace prefs
}  // namespace ash

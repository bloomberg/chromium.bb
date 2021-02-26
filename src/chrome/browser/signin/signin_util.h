// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_SIGNIN_UTIL_H_
#define CHROME_BROWSER_SIGNIN_SIGNIN_UTIL_H_

#include "base/supports_user_data.h"

class Profile;

namespace signin_util {

// TODO(crbug.com/1134111): Remove GuestSignedInUserData when Ephemeral Guest
// sign in functioncality is implemented.
class GuestSignedInUserData : public base::SupportsUserData::Data {
 public:
  static void SetIsSignedIn(Profile* profile, bool is_signed_in);
  static bool IsSignedIn(Profile* profile);

 private:
  static GuestSignedInUserData* GetForProfile(Profile* profile);

  bool is_signed_in_ = false;
};

// Return whether the force sign in policy is enabled or not.
// The state of this policy will not be changed without relaunch Chrome.
bool IsForceSigninEnabled();

// Enable or disable force sign in for testing.
void SetForceSigninForTesting(bool enable);

// Reset force sign in to uninitialized state for testing.
void ResetForceSigninForTesting();

// Returns true if clearing the primary profile is allowed.
bool IsUserSignoutAllowedForProfile(Profile* profile);

// Sign-out is allowed by default, but some Chrome profiles (e.g. for cloud-
// managed enterprise accounts) may wish to disallow user-initiated sign-out.
// Note that this exempts sign-outs that are not user-initiated (e.g. sign-out
// triggered when cloud policy no longer allows current email pattern). See
// ChromeSigninClient::PreSignOut().
void SetUserSignoutAllowedForProfile(Profile* profile, bool is_allowed);

// Updates the user sign-out state to |true| if is was never initialized.
// This should be called at the end of the flow to initialize a profile to
// ensure that the signout allowed flag is updated.
void EnsureUserSignoutAllowedIsInitializedForProfile(Profile* profile);

// Ensures that the primary account for |profile| is allowed:
// * If profile does not have any primary account, then this is a no-op.
// * If |IsUserSignoutAllowedForProfile| is allowed and the primary account
//   is no longer allowed, then this clears the primary account.
// * If |IsUserSignoutAllowedForProfile| is not allowed and the primary account
//   is not longer allowed, then this removes the profile.
void EnsurePrimaryAccountAllowedForProfile(Profile* profile);

}  // namespace signin_util

#endif  // CHROME_BROWSER_SIGNIN_SIGNIN_UTIL_H_

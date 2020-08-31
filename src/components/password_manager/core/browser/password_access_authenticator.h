// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_ACCESS_AUTHENTICATOR_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_ACCESS_AUTHENTICATOR_H_

#include <memory>

#include "base/callback.h"
#include "base/time/clock.h"
#include "base/time/time.h"
#include "components/password_manager/core/browser/reauth_purpose.h"

namespace password_manager {

// This class takes care of reauthentication used for accessing passwords
// through the settings page. It is used on all platforms but iOS and Android
// (see //ios/chrome/browser/ui/settings/reauthentication_module.* for iOS and
// PasswordEntryEditor.java and PasswordReauthenticationFragment.java in
// chrome/android/java/src/org/chromium/chrome/browser/preferences/password/
// for Android).
class PasswordAccessAuthenticator {
 public:
  using ReauthCallback = base::RepeatingCallback<bool(ReauthPurpose)>;

  // For how long after the last successful authentication a user is considered
  // authenticated without repeating the challenge.
  constexpr static base::TimeDelta kAuthValidityPeriod =
      base::TimeDelta::FromSeconds(60);

  // |os_reauth_call| is passed to |os_reauth_call_|, see the latter for
  // explanation.
  explicit PasswordAccessAuthenticator(ReauthCallback os_reauth_call);

  PasswordAccessAuthenticator(const PasswordAccessAuthenticator&) = delete;
  PasswordAccessAuthenticator& operator=(const PasswordAccessAuthenticator&) =
      delete;

  ~PasswordAccessAuthenticator();

  // Returns whether the user is able to pass the authentication challenge,
  // which is represented by |os_reauth_call_| returning true. A successful
  // result of |os_reauth_call_| is cached for kAuthValidityPeriod.
  bool EnsureUserIsAuthenticated(ReauthPurpose purpose);

  // Presents the reauthentication challenge to the user and returns whether
  // the user passed the challenge. This call is guaranteed to present the
  // challenge to the user.
  bool ForceUserReauthentication(ReauthPurpose purpose);

#if defined(UNIT_TEST)
  // Use this in tests to mock the OS-level reauthentication.
  void set_os_reauth_call(ReauthCallback os_reauth_call) {
    os_reauth_call_ = std::move(os_reauth_call);
  }
#endif  // defined(UNIT_TEST)

 private:
  // The last time the user was successfully authenticated.
  base::Time last_authentication_time_;

  // Used to directly present the authentication challenge (such as the login
  // prompt) to the user.
  ReauthCallback os_reauth_call_;
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_ACCESS_AUTHENTICATOR_H_

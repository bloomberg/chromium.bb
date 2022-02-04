// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SIGNIN_PROFILE_PICKER_LACROS_SIGN_IN_PROVIDER_H_
#define CHROME_BROWSER_UI_WEBUI_SIGNIN_PROFILE_PICKER_LACROS_SIGN_IN_PROVIDER_H_

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/lacros/account_manager/account_profile_mapper.h"
#include "chrome/browser/profiles/keep_alive/scoped_profile_keep_alive.h"
#include "chrome/browser/profiles/profile.h"
#include "components/signin/public/identity_manager/identity_manager.h"

struct CoreAccountInfo;

// Class responsible for showing the lacros sign-in dialog and returning a
// profile with a kSignin primary account.
class ProfilePickerLacrosSignInProvider
    : public signin::IdentityManager::Observer {
 public:
  // The callback returns the newly created profile and a valid WebContents
  // instance within this profile. If the flow gets canceled by closing the
  // window, the callback gets called with a nullptr.
  using SignedInCallback = base::OnceCallback<void(Profile* profile)>;

  ProfilePickerLacrosSignInProvider();
  ~ProfilePickerLacrosSignInProvider() override;
  ProfilePickerLacrosSignInProvider(const ProfilePickerLacrosSignInProvider&) =
      delete;
  ProfilePickerLacrosSignInProvider& operator=(
      const ProfilePickerLacrosSignInProvider&) = delete;

  // Opens an OS screen to add an account letting the user sign-in, creates a
  // new profile and assigns the newly added account to the profile as the
  // (unconsented) primary account. When any step in the process fails (such as
  // the user cancels account addition), it returns a nullptr profile.
  void ShowAddAccountDialogAndCreateSignedInProfile(SignedInCallback callback);

  // Creates a new profile with an existing account. Returns a nullptr profile
  // in case of failure.
  void CreateSignedInProfileWithExistingAccount(const std::string& gaia_id,
                                                SignedInCallback callback);

 private:
  // IdentityManager::Observer:
  void OnRefreshTokenUpdatedForAccount(
      const CoreAccountInfo& account_info) override;

  void OnLacrosProfileCreated(
      const absl::optional<AccountProfileMapper::AddAccountResult>& result);

  void OnLacrosAccountLoaded(const CoreAccountInfo& account);

  // Sign-in callback, valid until it's called.
  SignedInCallback callback_;

  Profile* profile_ = nullptr;
  // Prevent |profile_| from being destroyed first.
  std::unique_ptr<ScopedProfileKeepAlive> profile_keep_alive_;

  base::ScopedObservation<signin::IdentityManager,
                          signin::IdentityManager::Observer>
      identity_manager_observation_{this};

  base::WeakPtrFactory<ProfilePickerLacrosSignInProvider> weak_ptr_factory_{
      this};
};

#endif  // CHROME_BROWSER_UI_WEBUI_SIGNIN_PROFILE_PICKER_LACROS_SIGN_IN_PROVIDER_H_

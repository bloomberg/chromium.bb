// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SIGNIN_DICE_TURN_SYNC_ON_HELPER_DELEGATE_IMPL_H_
#define CHROME_BROWSER_UI_WEBUI_SIGNIN_DICE_TURN_SYNC_ON_HELPER_DELEGATE_IMPL_H_

#include "base/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "chrome/browser/ui/sync/profile_signin_confirmation_helper.h"
#include "chrome/browser/ui/webui/signin/dice_turn_sync_on_helper.h"
#include "chrome/browser/ui/webui/signin/login_ui_service.h"

class Browser;
class Profile;
class SigninUIError;
struct AccountInfo;

// Default implementation for DiceTurnSyncOnHelper::Delegate.
class DiceTurnSyncOnHelperDelegateImpl : public DiceTurnSyncOnHelper::Delegate,
                                         public BrowserListObserver,
                                         public LoginUIService::Observer {
 public:
  explicit DiceTurnSyncOnHelperDelegateImpl(Browser* browser);

  DiceTurnSyncOnHelperDelegateImpl(const DiceTurnSyncOnHelperDelegateImpl&) =
      delete;
  DiceTurnSyncOnHelperDelegateImpl& operator=(
      const DiceTurnSyncOnHelperDelegateImpl&) = delete;

  ~DiceTurnSyncOnHelperDelegateImpl() override;

 protected:
  void ShowEnterpriseAccountConfirmation(
      const AccountInfo& account_info,
      DiceTurnSyncOnHelper::SigninChoiceCallback callback) override;
  virtual void ShouldEnterpriseConfirmationPromptForNewProfile(
      Profile* profile,
      base::OnceCallback<void(bool)> callback);

 private:
  // DiceTurnSyncOnHelper::Delegate:
  void ShowLoginError(const SigninUIError& error) override;
  void ShowMergeSyncDataConfirmation(
      const std::string& previous_email,
      const std::string& new_email,
      DiceTurnSyncOnHelper::SigninChoiceCallback callback) override;
  void ShowSyncConfirmation(
      base::OnceCallback<void(LoginUIService::SyncConfirmationUIClosedResult)>
          callback) override;
  void ShowSyncDisabledConfirmation(
      bool is_managed_account,
      base::OnceCallback<void(LoginUIService::SyncConfirmationUIClosedResult)>
          callback) override;
  void ShowSyncSettings() override;
  void SwitchToProfile(Profile* new_profile) override;

  // LoginUIService::Observer:
  void OnSyncConfirmationUIClosed(
      LoginUIService::SyncConfirmationUIClosedResult result) override;

  // BrowserListObserver:
  void OnBrowserRemoved(Browser* browser) override;

  raw_ptr<Browser> browser_;
  raw_ptr<Profile> profile_;
  base::OnceCallback<void(LoginUIService::SyncConfirmationUIClosedResult)>
      sync_confirmation_callback_;
  base::ScopedObservation<LoginUIService, LoginUIService::Observer>
      scoped_login_ui_service_observation_{this};
};

#endif  // CHROME_BROWSER_UI_WEBUI_SIGNIN_DICE_TURN_SYNC_ON_HELPER_DELEGATE_IMPL_H_

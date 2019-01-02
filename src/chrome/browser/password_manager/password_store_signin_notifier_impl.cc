// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/password_store_signin_notifier_impl.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/account_tracker_service_factory.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "components/signin/core/browser/signin_manager.h"

namespace password_manager {

PasswordStoreSigninNotifierImpl::PasswordStoreSigninNotifierImpl(
    Profile* profile)
    : profile_(profile) {
  DCHECK(profile);
}

PasswordStoreSigninNotifierImpl::~PasswordStoreSigninNotifierImpl() {}

void PasswordStoreSigninNotifierImpl::SubscribeToSigninEvents(
    PasswordStore* store) {
  set_store(store);
  SigninManagerFactory::GetForProfile(profile_)->AddObserver(this);
  AccountTrackerServiceFactory::GetForProfile(profile_)->AddObserver(this);
}

void PasswordStoreSigninNotifierImpl::UnsubscribeFromSigninEvents() {
  SigninManagerFactory::GetForProfile(profile_)->RemoveObserver(this);
  AccountTrackerServiceFactory::GetForProfile(profile_)->RemoveObserver(this);
}

void PasswordStoreSigninNotifierImpl::GoogleSigninSucceededWithPassword(
    const std::string& account_id,
    const std::string& username,
    const std::string& password) {
  NotifySignin(username, password);
}

void PasswordStoreSigninNotifierImpl::GoogleSignedOut(
    const std::string& account_id,
    const std::string& username) {
  NotifySignedOut(username, /* primary_account= */ true);
}

// AccountTrackerService::Observer implementations.
void PasswordStoreSigninNotifierImpl::OnAccountRemoved(
    const AccountInfo& info) {
  // Only reacts to content area (non-primary) Gaia account sign-out event.
  if (info.account_id != SigninManagerFactory::GetForProfile(profile_)
                             ->GetAuthenticatedAccountId()) {
    NotifySignedOut(info.email, /* primary_account= */ false);
  }
}

}  // namespace password_manager

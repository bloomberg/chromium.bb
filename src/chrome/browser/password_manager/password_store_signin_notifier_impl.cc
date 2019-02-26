// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/password_store_signin_notifier_impl.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/account_tracker_service_factory.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "services/identity/public/cpp/identity_manager.h"

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
  IdentityManagerFactory::GetForProfile(profile_)->AddObserver(this);
  AccountTrackerServiceFactory::GetForProfile(profile_)->AddObserver(this);
}

void PasswordStoreSigninNotifierImpl::UnsubscribeFromSigninEvents() {
  AccountTrackerServiceFactory::GetForProfile(profile_)->RemoveObserver(this);
  IdentityManagerFactory::GetForProfile(profile_)->RemoveObserver(this);
}

void PasswordStoreSigninNotifierImpl::OnPrimaryAccountSetWithPassword(
    const AccountInfo& account_info,
    const std::string& password) {
  NotifySignin(account_info.email, password);
}

void PasswordStoreSigninNotifierImpl::OnPrimaryAccountCleared(
    const AccountInfo& account_info) {
  NotifySignedOut(account_info.email, /* primary_account= */ true);
}

// AccountTrackerService::Observer implementations.
void PasswordStoreSigninNotifierImpl::OnAccountRemoved(
    const AccountInfo& info) {
  // Only reacts to content area (non-primary) Gaia account sign-out event.
  if (info.account_id !=
      IdentityManagerFactory::GetForProfile(profile_)->GetPrimaryAccountId()) {
    NotifySignedOut(info.email, /* primary_account= */ false);
  }
}

}  // namespace password_manager

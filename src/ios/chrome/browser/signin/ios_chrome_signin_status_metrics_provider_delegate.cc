// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/signin/ios_chrome_signin_status_metrics_provider_delegate.h"

#include "components/signin/core/browser/signin_status_metrics_provider.h"
#include "ios/chrome/browser/application_context.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state_manager.h"
#include "ios/chrome/browser/signin/identity_manager_factory.h"
#include "services/identity/public/cpp/identity_manager.h"

IOSChromeSigninStatusMetricsProviderDelegate::
    IOSChromeSigninStatusMetricsProviderDelegate() {}

IOSChromeSigninStatusMetricsProviderDelegate::
    ~IOSChromeSigninStatusMetricsProviderDelegate() {
  IdentityManagerFactory* factory = IdentityManagerFactory::GetInstance();
  if (factory)
    factory->RemoveObserver(this);
}

void IOSChromeSigninStatusMetricsProviderDelegate::Initialize() {
  IdentityManagerFactory* factory = IdentityManagerFactory::GetInstance();
  if (factory)
    factory->AddObserver(this);
}

AccountsStatus
IOSChromeSigninStatusMetricsProviderDelegate::GetStatusOfAllAccounts() {
  std::vector<ios::ChromeBrowserState*> browser_state_list =
      GetLoadedChromeBrowserStates();
  AccountsStatus accounts_status;
  accounts_status.num_accounts = browser_state_list.size();
  accounts_status.num_opened_accounts = accounts_status.num_accounts;

  for (ios::ChromeBrowserState* browser_state : browser_state_list) {
    auto* manager = IdentityManagerFactory::GetForBrowserState(
        browser_state->GetOriginalChromeBrowserState());
    if (manager && manager->HasPrimaryAccount())
      accounts_status.num_signed_in_accounts++;
  }

  return accounts_status;
}

std::vector<identity::IdentityManager*>
IOSChromeSigninStatusMetricsProviderDelegate::
    GetIdentityManagersForAllAccounts() {
  std::vector<identity::IdentityManager*> managers;
  for (ios::ChromeBrowserState* browser_state :
       GetLoadedChromeBrowserStates()) {
    identity::IdentityManager* manager =
        IdentityManagerFactory::GetForBrowserStateIfExists(browser_state);
    if (manager) {
      managers.push_back(manager);
    }
  }

  return managers;
}

void IOSChromeSigninStatusMetricsProviderDelegate::IdentityManagerCreated(
    identity::IdentityManager* manager) {
  owner()->OnIdentityManagerCreated(manager);
}

void IOSChromeSigninStatusMetricsProviderDelegate::IdentityManagerShutdown(
    identity::IdentityManager* manager) {
  owner()->OnIdentityManagerShutdown(manager);
}

std::vector<ios::ChromeBrowserState*>
IOSChromeSigninStatusMetricsProviderDelegate::GetLoadedChromeBrowserStates() {
  return GetApplicationContext()
      ->GetChromeBrowserStateManager()
      ->GetLoadedBrowserStates();
}

// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/signin/signin_manager_factory.h"

#include <utility>

#include "base/no_destructor.h"
#include "base/time/time.h"
#include "components/keyed_service/ios/browser_state_dependency_manager.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/signin/core/browser/account_consistency_method.h"
#include "components/signin/core/browser/signin_manager.h"
#include "components/signin/core/browser/signin_pref_names.h"
#include "ios/chrome/browser/application_context.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/signin/account_tracker_service_factory.h"
#include "ios/chrome/browser/signin/gaia_cookie_manager_service_factory.h"
#include "ios/chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "ios/chrome/browser/signin/signin_client_factory.h"

namespace ios {

SigninManagerFactory::SigninManagerFactory()
    : BrowserStateKeyedServiceFactory(
          "SigninManager",
          BrowserStateDependencyManager::GetInstance()) {
  DependsOn(SigninClientFactory::GetInstance());
  DependsOn(ios::GaiaCookieManagerServiceFactory::GetInstance());
  DependsOn(ProfileOAuth2TokenServiceFactory::GetInstance());
  DependsOn(ios::AccountTrackerServiceFactory::GetInstance());
}

SigninManagerFactory::~SigninManagerFactory() {}

// static
SigninManager* SigninManagerFactory::GetForBrowserState(
    ios::ChromeBrowserState* browser_state) {
  return static_cast<SigninManager*>(
      GetInstance()->GetServiceForBrowserState(browser_state, true));
}

// static
SigninManager* SigninManagerFactory::GetForBrowserStateIfExists(
    ios::ChromeBrowserState* browser_state) {
  return static_cast<SigninManager*>(
      GetInstance()->GetServiceForBrowserState(browser_state, false));
}

// static
SigninManagerFactory* SigninManagerFactory::GetInstance() {
  static base::NoDestructor<SigninManagerFactory> instance;
  return instance.get();
}

void SigninManagerFactory::RegisterBrowserStatePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  SigninManagerBase::RegisterProfilePrefs(registry);
}

std::unique_ptr<KeyedService> SigninManagerFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  ios::ChromeBrowserState* chrome_browser_state =
      ios::ChromeBrowserState::FromBrowserState(context);
  std::unique_ptr<SigninManager> service(new SigninManager(
      SigninClientFactory::GetForBrowserState(chrome_browser_state),
      ProfileOAuth2TokenServiceFactory::GetForBrowserState(
          chrome_browser_state),
      ios::AccountTrackerServiceFactory::GetForBrowserState(
          chrome_browser_state),
      ios::GaiaCookieManagerServiceFactory::GetForBrowserState(
          chrome_browser_state),
      signin::AccountConsistencyMethod::kMirror));
  service->Initialize(GetApplicationContext()->GetLocalState());
  return service;
}

}  // namespace ios

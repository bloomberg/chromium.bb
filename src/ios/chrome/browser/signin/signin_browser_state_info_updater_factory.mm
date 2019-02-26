// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/signin/signin_browser_state_info_updater_factory.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

#include "components/keyed_service/ios/browser_state_dependency_manager.h"
#include "components/signin/core/browser/signin_manager.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/signin/signin_browser_state_info_updater.h"
#include "ios/chrome/browser/signin/signin_error_controller_factory.h"
#include "ios/chrome/browser/signin/signin_manager_factory.h"

// static
SigninBrowserStateInfoUpdater*
SigninBrowserStateInfoUpdaterFactory::GetForBrowserState(
    ios::ChromeBrowserState* chrome_browser_state) {
  return static_cast<SigninBrowserStateInfoUpdater*>(
      GetInstance()->GetServiceForBrowserState(chrome_browser_state, true));
}

// static
SigninBrowserStateInfoUpdaterFactory*
SigninBrowserStateInfoUpdaterFactory::GetInstance() {
  return base::Singleton<SigninBrowserStateInfoUpdaterFactory>::get();
}

SigninBrowserStateInfoUpdaterFactory::SigninBrowserStateInfoUpdaterFactory()
    : BrowserStateKeyedServiceFactory(
          "SigninBrowserStateInfoUpdater",
          BrowserStateDependencyManager::GetInstance()) {
  DependsOn(ios::SigninErrorControllerFactory::GetInstance());
  DependsOn(ios::SigninManagerFactory::GetInstance());
}

SigninBrowserStateInfoUpdaterFactory::~SigninBrowserStateInfoUpdaterFactory() {}

std::unique_ptr<KeyedService>
SigninBrowserStateInfoUpdaterFactory::BuildServiceInstanceFor(
    web::BrowserState* state) const {
  ios::ChromeBrowserState* chrome_browser_state =
      ios::ChromeBrowserState::FromBrowserState(state);
  return std::make_unique<SigninBrowserStateInfoUpdater>(
      ios::SigninManagerFactory::GetForBrowserState(chrome_browser_state),
      ios::SigninErrorControllerFactory::GetForBrowserState(
          chrome_browser_state),
      chrome_browser_state->GetStatePath());
}

bool SigninBrowserStateInfoUpdaterFactory::ServiceIsCreatedWithBrowserState()
    const {
  return true;
}

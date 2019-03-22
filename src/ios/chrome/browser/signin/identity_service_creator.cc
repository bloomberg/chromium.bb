// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/signin/identity_service_creator.h"

#include <memory>

#include "components/signin/core/browser/signin_manager.h"
#include "ios/chrome/browser/signin/account_tracker_service_factory.h"
#include "ios/chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "ios/chrome/browser/signin/signin_manager_factory.h"
#include "services/identity/identity_service.h"

std::unique_ptr<service_manager::Service> CreateIdentityService(
    ios::ChromeBrowserState* browser_state,
    service_manager::mojom::ServiceRequest request) {
  AccountTrackerService* account_tracker =
      ios::AccountTrackerServiceFactory::GetForBrowserState(browser_state);
  SigninManagerBase* signin_manager =
      ios::SigninManagerFactory::GetForBrowserState(browser_state);
  ProfileOAuth2TokenService* token_service =
      ProfileOAuth2TokenServiceFactory::GetForBrowserState(browser_state);
  return std::make_unique<identity::IdentityService>(
      account_tracker, signin_manager, token_service, std::move(request));
}

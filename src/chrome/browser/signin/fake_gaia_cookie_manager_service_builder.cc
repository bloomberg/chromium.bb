// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/fake_gaia_cookie_manager_service_builder.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/chrome_signin_client_factory.h"
#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/signin/core/browser/fake_gaia_cookie_manager_service.h"
#include "components/signin/core/browser/profile_oauth2_token_service.h"
#include "google_apis/gaia/gaia_constants.h"

namespace {

std::unique_ptr<KeyedService> BuildFakeGaiaCookieManagerServiceImpl(
    content::BrowserContext* context,
    bool use_fake_url_fetcher) {
  Profile* profile = Profile::FromBrowserContext(context);
  return std::make_unique<FakeGaiaCookieManagerService>(
      ProfileOAuth2TokenServiceFactory::GetForProfile(profile),
      GaiaConstants::kChromeSource,
      ChromeSigninClientFactory::GetForProfile(profile), use_fake_url_fetcher);
}

}  // namespace

std::unique_ptr<KeyedService> BuildFakeGaiaCookieManagerService(
    content::BrowserContext* context) {
  return BuildFakeGaiaCookieManagerServiceImpl(context,
                                               /*use_fake_url_fetcher=*/true);
}

std::unique_ptr<KeyedService> BuildFakeGaiaCookieManagerServiceNoFakeUrlFetcher(
    content::BrowserContext* context) {
  return BuildFakeGaiaCookieManagerServiceImpl(context,
                                               /*use_fake_url_fetcher=*/false);
}

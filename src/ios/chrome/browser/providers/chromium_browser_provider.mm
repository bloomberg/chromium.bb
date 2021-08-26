// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/providers/chromium_browser_provider.h"

#include <memory>

#import "ios/chrome/browser/providers/chromium_logo_controller.h"
#import "ios/chrome/browser/providers/chromium_voice_search_provider.h"
#include "ios/chrome/browser/providers/signin/chromium_signin_resources_provider.h"
#import "ios/public/provider/chrome/browser/discover_feed/discover_feed_provider.h"
#include "ios/public/provider/chrome/browser/overrides_provider.h"
#include "ios/public/provider/chrome/browser/signin/chrome_identity_service.h"
#include "ios/public/provider/chrome/browser/user_feedback/user_feedback_provider.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

ChromiumBrowserProvider::ChromiumBrowserProvider()
    : signin_resources_provider_(
          std::make_unique<ChromiumSigninResourcesProvider>()),
      user_feedback_provider_(std::make_unique<UserFeedbackProvider>()),
      voice_search_provider_(std::make_unique<ChromiumVoiceSearchProvider>()),
      overrides_provider_(std::make_unique<OverridesProvider>()),
      discover_feed_provider_(std::make_unique<DiscoverFeedProvider>()) {}

ChromiumBrowserProvider::~ChromiumBrowserProvider() {}

ios::SigninResourcesProvider*
ChromiumBrowserProvider::GetSigninResourcesProvider() {
  return signin_resources_provider_.get();
}

void ChromiumBrowserProvider::SetChromeIdentityServiceForTesting(
    std::unique_ptr<ios::ChromeIdentityService> service) {
  chrome_identity_service_ = std::move(service);
}

ios::ChromeIdentityService*
ChromiumBrowserProvider::GetChromeIdentityService() {
  if (!chrome_identity_service_) {
    chrome_identity_service_ = std::make_unique<ios::ChromeIdentityService>();
  }
  return chrome_identity_service_.get();
}

UITextField* ChromiumBrowserProvider::CreateStyledTextField() const {
  return [[UITextField alloc] initWithFrame:CGRectZero];
}

VoiceSearchProvider* ChromiumBrowserProvider::GetVoiceSearchProvider() const {
  return voice_search_provider_.get();
}

id<LogoVendor> ChromiumBrowserProvider::CreateLogoVendor(
    Browser* browser,
    web::WebState* web_state) const {
  return [[ChromiumLogoController alloc] init];
}

UserFeedbackProvider* ChromiumBrowserProvider::GetUserFeedbackProvider() const {
  return user_feedback_provider_.get();
}

OverridesProvider* ChromiumBrowserProvider::GetOverridesProvider() const {
  return overrides_provider_.get();
}

DiscoverFeedProvider* ChromiumBrowserProvider::GetDiscoverFeedProvider() const {
  return discover_feed_provider_.get();
}

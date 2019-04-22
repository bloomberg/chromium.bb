// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/web_view/internal/signin/web_view_signin_error_controller_factory.h"

#include <utility>

#include "base/no_destructor.h"
#include "components/keyed_service/core/service_access_type.h"
#include "components/keyed_service/ios/browser_state_dependency_manager.h"
#include "components/signin/core/browser/signin_error_controller.h"
#include "ios/web_view/internal/signin/web_view_identity_manager_factory.h"
#include "ios/web_view/internal/web_view_browser_state.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace ios_web_view {

// static
SigninErrorController* WebViewSigninErrorControllerFactory::GetForBrowserState(
    ios_web_view::WebViewBrowserState* browser_state) {
  return static_cast<SigninErrorController*>(
      GetInstance()->GetServiceForBrowserState(browser_state, true));
}

// static
WebViewSigninErrorControllerFactory*
WebViewSigninErrorControllerFactory::GetInstance() {
  static base::NoDestructor<WebViewSigninErrorControllerFactory> instance;
  return instance.get();
}

WebViewSigninErrorControllerFactory::WebViewSigninErrorControllerFactory()
    : BrowserStateKeyedServiceFactory(
          "SigninErrorController",
          BrowserStateDependencyManager::GetInstance()) {
  DependsOn(WebViewIdentityManagerFactory::GetInstance());
}

std::unique_ptr<KeyedService>
WebViewSigninErrorControllerFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  WebViewBrowserState* browser_state =
      WebViewBrowserState::FromBrowserState(context);
  return std::make_unique<SigninErrorController>(
      SigninErrorController::AccountMode::ANY_ACCOUNT,
      WebViewIdentityManagerFactory::GetForBrowserState(browser_state));
}

}  // namespace ios_web_view

// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_VIEW_INTERNAL_PASSWORDS_WEB_VIEW_PASSWORD_MANAGER_INTERNALS_SERVICE_FACTORY_H_
#define IOS_WEB_VIEW_INTERNAL_PASSWORDS_WEB_VIEW_PASSWORD_MANAGER_INTERNALS_SERVICE_FACTORY_H_

#include "base/macros.h"
#include "base/no_destructor.h"
#include "components/keyed_service/ios/browser_state_keyed_service_factory.h"

namespace password_manager {
class PasswordManagerInternalsService;
}

namespace ios_web_view {
class WebViewBrowserState;

// Singleton that owns all PasswordStores and associates them with
// WebViewBrowserState.
class WebViewPasswordManagerInternalsServiceFactory
    : public BrowserStateKeyedServiceFactory {
 public:
  static password_manager::PasswordManagerInternalsService* GetForBrowserState(
      WebViewBrowserState* browser_state);

  static WebViewPasswordManagerInternalsServiceFactory* GetInstance();

 private:
  friend class base::NoDestructor<
      WebViewPasswordManagerInternalsServiceFactory>;

  WebViewPasswordManagerInternalsServiceFactory();
  ~WebViewPasswordManagerInternalsServiceFactory() override;

  // BrowserStateKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;

  DISALLOW_COPY_AND_ASSIGN(WebViewPasswordManagerInternalsServiceFactory);
};

}  // namespace ios_web_view

#endif  // IOS_WEB_VIEW_INTERNAL_PASSWORDS_WEB_VIEW_PASSWORD_MANAGER_INTERNALS_SERVICE_FACTORY_H_

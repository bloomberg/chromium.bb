// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_VIEW_INTERNAL_LANGUAGE_WEB_VIEW_LANGUAGE_MODEL_MANAGER_FACTORY_H_
#define IOS_WEB_VIEW_INTERNAL_LANGUAGE_WEB_VIEW_LANGUAGE_MODEL_MANAGER_FACTORY_H_

#include <memory>

#include "base/macros.h"
#include "base/no_destructor.h"
#include "components/keyed_service/ios/browser_state_keyed_service_factory.h"

namespace language {
class LanguageModelManager;
}  // namespace language

namespace user_prefs {
class PrefRegistrySyncable;
}  // namespace user_prefs

namespace ios_web_view {

class WebViewBrowserState;

class WebViewLanguageModelManagerFactory
    : public BrowserStateKeyedServiceFactory {
 public:
  static language::LanguageModelManager* GetForBrowserState(
      WebViewBrowserState* browser_state);
  static WebViewLanguageModelManagerFactory* GetInstance();

 private:
  friend class base::NoDestructor<WebViewLanguageModelManagerFactory>;

  WebViewLanguageModelManagerFactory();
  ~WebViewLanguageModelManagerFactory() override = default;

  // BrowserStateKeyedServiceFactory implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;
  void RegisterBrowserStatePrefs(
      user_prefs::PrefRegistrySyncable* const registry) override;
  web::BrowserState* GetBrowserStateToUse(
      web::BrowserState* state) const override;

  DISALLOW_COPY_AND_ASSIGN(WebViewLanguageModelManagerFactory);
};

}  // namespace ios_web_view

#endif  // IOS_WEB_VIEW_INTERNAL_LANGUAGE_WEB_VIEW_LANGUAGE_MODEL_MANAGER_FACTORY_H_

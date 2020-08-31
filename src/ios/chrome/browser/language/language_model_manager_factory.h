// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_LANGUAGE_LANGUAGE_MODEL_MANAGER_FACTORY_H_
#define IOS_CHROME_BROWSER_LANGUAGE_LANGUAGE_MODEL_MANAGER_FACTORY_H_

#include <memory>

#include "base/macros.h"
#include "base/no_destructor.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/keyed_service/ios/browser_state_keyed_service_factory.h"

class ChromeBrowserState;

namespace language {
class LanguageModelManager;
}

namespace user_prefs {
class PrefRegistrySyncable;
}

// Manages the language model for each profile. The particular language model
// provided depends on feature flags.
class LanguageModelManagerFactory : public BrowserStateKeyedServiceFactory {
 public:
  static LanguageModelManagerFactory* GetInstance();
  static language::LanguageModelManager* GetForBrowserState(
      ChromeBrowserState* browser_state);

 private:
  friend class base::NoDestructor<LanguageModelManagerFactory>;

  LanguageModelManagerFactory();
  ~LanguageModelManagerFactory() override;

  // BrowserStateKeyedServiceFactory implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;
  web::BrowserState* GetBrowserStateToUse(
      web::BrowserState* state) const override;
  void RegisterBrowserStatePrefs(
      user_prefs::PrefRegistrySyncable* registry) override;

  DISALLOW_COPY_AND_ASSIGN(LanguageModelManagerFactory);
};

#endif  // IOS_CHROME_BROWSER_LANGUAGE_LANGUAGE_MODEL_MANAGER_FACTORY_H_
